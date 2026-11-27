struct PendingPlayerControl {
    bool active = false;
    bool walkTo = false;
    bool inputStateSaved = false;
    std::string id;
    std::string command;
    zCWorld* world = nullptr;
    oCNpc* player = nullptr;
    unsigned short inputKey = 0;
    int savedInputState = 0;
    std::uint64_t controlAiFrames = 0;
    std::uint64_t startMs = 0;
    std::uint64_t endMs = 0;
    std::uint64_t lastProgressMs = 0;
    std::uint64_t requestDeadlineMs = 0;
    std::uint64_t stallLimitMs = 0;
    float lastDistance = 0.0f;
    float tolerance = 0.0f;
    zVEC3 startPosition;
    zVEC3 targetPosition;
};
PendingPlayerControl pendingPlayerControl;

void __fastcall BridgePlayerHumanDoAI(oCAIHuman* self, void* edx, zCVob* vob, int& result);
auto PlayerHumanDoAIHook = Union::CreateHook(
    SIGNATURE_OF(&oCAIHuman::DoAI), &BridgePlayerHumanDoAI, Union::HookType::Hook_Detours);

void __fastcall BridgePlayerHumanDoAI(oCAIHuman* self, void* edx, zCVob* vob, int& result) {
    oCNpc* player = ogame ? ogame->GetSelfPlayerVob() : nullptr;
    const bool inject = pendingPlayerControl.active && self && vob && player &&
        vob == player && player == pendingPlayerControl.player &&
        self == player->human_ai && !player->IsDead() && !player->IsUnconscious() &&
        currentWorld() == pendingPlayerControl.world && zinput;
    if (!inject) {
        PlayerHumanDoAIHook(self, edx, vob, result);
        return;
    }

    const unsigned short inputKey = pendingPlayerControl.inputKey;
    const int savedState = zinput->GetState(inputKey) > 0.0f ? 1 : 0;
    zinput->SetState(inputKey, 1);
    ++pendingPlayerControl.controlAiFrames;
    PlayerHumanDoAIHook(self, edx, vob, result);
    if (zinput) zinput->SetState(inputKey, savedState);
}

struct PendingScreenshot {
    bool active = false;
    std::string id;
    std::string filename;
};
PendingScreenshot pendingScreenshot;

struct PendingMonitor {
    bool active = false;
    std::string id;
    std::string targetType;
    std::string targetName;
    std::string referenceName;
    zCWorld* world = nullptr;
    oCNpc* initialPlayer = nullptr;
    std::uint64_t startMs = 0;
    std::uint64_t endMs = 0;
    std::uint64_t nextSampleMs = 0;
    std::uint64_t requestDeadlineMs = 0;
    std::uint64_t stallLimitMs = 0;
    std::uint64_t lastProgressMs = 0;
    int intervalMs = 0;
    int maxSamples = 0;
    float lastMovedDistance = 0.0f;
    zVEC3 lastPosition;
    bool hasPosition = false;
    bool stalled = false;
    std::vector<json> samples;
};
PendingMonitor pendingMonitor;

std::string vobNameUtf8(zCVob* vob) {
    return vob ? transcode(vob->GetObjectName().ToChar(), 1250, CP_UTF8) : std::string{};
}

zVEC3 vobPosition(zCVob* vob) {
    return vob ? vob->trafoObjToWorld.GetTranslation() : zVEC3(0, 0, 0);
}

float distanceBetween(const zVEC3& a, const zVEC3& b) {
    const float dx = a.n[0] - b.n[0];
    const float dy = a.n[1] - b.n[1];
    const float dz = a.n[2] - b.n[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool unitHorizontalDirection(const zVEC3& direction, zVEC3& unit) {
    const float lengthSquared = direction.n[0] * direction.n[0] +
                                direction.n[2] * direction.n[2];
    if (!std::isfinite(lengthSquared) || lengthSquared < 0.01f) return false;
    const float length = std::sqrt(lengthSquared);
    if (!std::isfinite(length) || length < 0.1f) return false;
    unit = zVEC3(direction.n[0] / length, 0.0f, direction.n[2] / length);
    return true;
}

json vobSummary(zCVob* vob) {
    if (!vob) return nullptr;
    const zVEC3 position = vobPosition(vob);
    const zVEC3 size(vob->bbox3D.maxs.n[0] - vob->bbox3D.mins.n[0],
                     vob->bbox3D.maxs.n[1] - vob->bbox3D.mins.n[1],
                     vob->bbox3D.maxs.n[2] - vob->bbox3D.mins.n[2]);
    json result = {
        {"name", vobNameUtf8(vob)},
        {"class", className(vob)},
        {"position", vec(position)},
        {"orientation", json{{"right", vec(vob->GetRightVectorWorld())},
                              {"up", vec(vob->GetUpVectorWorld())},
                              {"at", vec(vob->GetAtVectorWorld())}}},
        {"matrix", matrixJson(vob->trafoObjToWorld)},
        {"bbox3d_min", vec(vob->bbox3D.mins)},
        {"bbox3d_max", vec(vob->bbox3D.maxs)},
        {"bbox3d_size", vec(size)},
        {"collision", json{{"static", static_cast<bool>(vob->collDetectionStatic)},
                            {"dynamic", static_cast<bool>(vob->collDetectionDynamic)},
                            {"object_present", vob->GetCollisionObject() != nullptr}}},
        {"visual", nullptr}
    };
    if (vob->visual) {
        const zSTRING visualName = vob->visual->GetVisualName();
        result["visual"] = json{{"name", transcode(visualName.ToChar(), 1250, CP_UTF8)}};
    }
    return result;
}

zCVob* findVobByName(zCWorld* world, const std::string& utf8Name) {
    if (!world || utf8Name.empty() || utf8Name.size() > 128 || utf8Name.find('\0') != std::string::npos)
        return nullptr;
    const std::string encoded = transcode(utf8Name, CP_UTF8, 1250);
    return world->SearchVobByName(zSTRING(encoded.c_str()));
}

int boundedRequestInt(const json& request, const char* field, int fallback,
                      int minimum, int maximum) {
    if (!request.contains(field)) return fallback;
    const json& raw = request.at(field);
    if (!raw.is_number_integer())
        fail("invalid_request", std::string(field) + " must be an integer");
    const std::int64_t parsed = raw.get<std::int64_t>();
    if (parsed < minimum || parsed > maximum)
        fail("invalid_request", std::string(field) + " is outside the supported range");
    return static_cast<int>(parsed);
}

float boundedRequestFloat(const json& request, const char* field, float fallback,
                          float minimum, float maximum) {
    if (!request.contains(field)) return fallback;
    const json& raw = request.at(field);
    if (!raw.is_number()) fail("invalid_request", std::string(field) + " must be a number");
    const float value = raw.get<float>();
    if (!std::isfinite(value) || value < minimum || value > maximum)
        fail("invalid_request", std::string(field) + " is outside the supported range");
    return value;
}

json vobSearch(const json& request, zCWorld* world) {
    if (!world) fail("world_not_ready", "no world is currently loaded");
    const std::string nameExact = request.value("name_exact", std::string{});
    const std::string nameContains = request.value("name_contains", std::string{});
    const std::string classContains = request.value("class_contains", std::string{});
    for (const std::string* filter : {&nameExact, &nameContains, &classContains})
        if (filter->size() > 128 || filter->find('\0') != std::string::npos)
            fail("invalid_request", "VOb search filters must be at most 128 characters");

    const int limit = boundedRequestInt(request, "limit", 25, 1, 100);
    const int offset = boundedRequestInt(request, "offset", 0, 0, 100000);
    const int scanLimit = boundedRequestInt(request, "scan_limit", 20000, 1, 200000);
    const bool hasCenter = request.contains("center") || request.contains("center_vob");
    if (request.contains("center") && request.contains("center_vob"))
        fail("invalid_request", "provide center or center_vob, not both");
    zVEC3 center(0, 0, 0);
    if (request.contains("center")) center = readVec(request.at("center"));
    if (request.contains("center_vob")) {
        if (!request["center_vob"].is_string()) fail("invalid_request", "center_vob must be a string");
        zCVob* centerVob = findVobByName(world, request["center_vob"].get<std::string>());
        if (!centerVob) fail("vob_not_found", "center VOb was not found");
        center = vobPosition(centerVob);
    }
    float radius = 0.0f;
    if (request.contains("radius")) {
        radius = request.at("radius").get<float>();
        if (!std::isfinite(radius) || radius < 0.0f || radius > 100000.0f || !hasCenter)
            fail("invalid_request", "radius must be 0..100000 and requires center or center_vob");
    }
    const std::string sortBy = request.value("sort_by", std::string{});
    if (!sortBy.empty() && sortBy != "distance") fail("invalid_request", "sort_by supports only distance");
    if (sortBy == "distance" && !hasCenter) fail("invalid_request", "distance sorting requires center or center_vob");

    zCArray<zCVob*> vobs;
    if (!zCVob::classDef) fail("vob_search_unavailable", "VOb class metadata is unavailable");
    world->SearchVobListByBaseClass(zCVob::classDef, vobs, &world->globalVobTree);
    const int totalVobs = std::max(0, vobs.numInArray);
    const int effectiveLimit = std::min(scanLimit, totalVobs);
    struct Match { zCVob* vob; float distance; };
    std::vector<Match> matches;
    int scanned = 0;
    const auto scanStart = std::chrono::steady_clock::now();
    bool timeLimited = false;
    while (scanned < effectiveLimit) {
        if ((scanned & 63) == 0) {
            checkRequestControl(requestState.id, requestState.deadlineMs);
            if (std::chrono::steady_clock::now() - scanStart >= std::chrono::milliseconds(15)) {
                timeLimited = true;
                break;
            }
        }
        zCVob* vob = vobs[scanned++];
        if (vob) {
            const std::string name = vobNameUtf8(vob);
            const std::string type = className(vob);
            const bool nameMatches = (nameExact.empty() || name == nameExact) &&
                (nameContains.empty() || name.find(nameContains) != std::string::npos);
            const bool typeMatches = classContains.empty() || type.find(classContains) != std::string::npos;
            const float distance = hasCenter ? distanceBetween(vobPosition(vob), center) : 0.0f;
            if (nameMatches && typeMatches && (!request.contains("radius") || distance <= radius))
                matches.push_back(Match{vob, distance});
        }
    }
    if (sortBy == "distance")
        std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
            return a.distance < b.distance;
        });

    json rows = json::array();
    const size_t begin = std::min(static_cast<size_t>(offset), matches.size());
    const size_t end = std::min(matches.size(), begin + static_cast<size_t>(limit));
    for (size_t i = begin; i < end; ++i) {
        json row = vobSummary(matches[i].vob);
        if (hasCenter) row["distance"] = matches[i].distance;
        rows.push_back(std::move(row));
    }
    return json{{"ok", true}, {"results", std::move(rows)}, {"count", end - begin},
                {"matches_scanned", matches.size()}, {"nodes_scanned", scanned},
                {"total_vobs", totalVobs}, {"scan_limit", scanLimit}, {"scan_truncated", scanned < totalVobs},
                {"scan_time_limited", timeLimited},
                {"offset", offset}, {"limit", limit},
                {"sort_by", sortBy.empty() ? "tree_order" : sortBy}};
}

json vobHierarchy(const json& request, zCWorld* world) {
    if (!world) fail("world_not_ready", "no world is currently loaded");
    if (!request.contains("name") || !request["name"].is_string())
        fail("invalid_request", "vob_hierarchy requires a VOb name");
    zCVob* target = findVobByName(world, request["name"].get<std::string>());
    if (!target) fail("vob_not_found", "VOb was not found");
    const int depthLimit = boundedRequestInt(request, "children_depth", 2, 0, 8);
    const int nodeLimit = boundedRequestInt(request, "max_nodes", 64, 1, 256);
    zCTree<zCVob>* targetNode = target->globalVobTreeNode;
    if (!targetNode || targetNode->GetData() != target)
        fail("vob_hierarchy_unavailable", "VOb tree node is unavailable or changed");

    std::vector<zCVob*> ancestors;
    for (zCTree<zCVob>* node = targetNode->GetParent(); node && ancestors.size() < 64; node = node->GetParent())
        if (node->GetData()) ancestors.push_back(node->GetData());
    std::reverse(ancestors.begin(), ancestors.end());
    json ancestorRows = json::array();
    for (zCVob* vob : ancestors) ancestorRows.push_back(vobSummary(vob));

    struct Child { zCTree<zCVob>* node; int depth; };
    std::vector<Child> stack;
    for (zCTree<zCVob>* child = targetNode->GetFirstChild(); child; child = child->GetNextChild())
        stack.push_back(Child{child, 1});
    json childRows = json::array();
    int scanned = 0;
    while (!stack.empty() && scanned < nodeLimit) {
        const Child item = stack.back();
        stack.pop_back();
        ++scanned;
        if (item.node->GetData()) {
            json row = vobSummary(item.node->GetData());
            row["depth"] = item.depth;
            childRows.push_back(std::move(row));
        }
        if (item.depth < depthLimit)
            for (zCTree<zCVob>* child = item.node->GetFirstChild(); child; child = child->GetNextChild())
                stack.push_back(Child{child, item.depth + 1});
    }
    return json{{"ok", true}, {"target", vobSummary(target)}, {"ancestors", std::move(ancestorRows)},
                {"children", std::move(childRows)}, {"children_nodes_scanned", scanned},
                {"children_truncated", !stack.empty()}, {"children_depth", depthLimit}};
}

json vobReference(zCVob* vob) {
    if (!vob) return nullptr;
    return json{{"name", vobNameUtf8(vob)}, {"class", className(vob)}, {"position", vec(vobPosition(vob))}};
}

json inspectNpc(oCNpc* npc, zCWorld* world, int inventoryLimit = 8) {
    if (!npc) fail("npc_not_found", "NPC was not found");
    const zVEC3 position = vobPosition(npc);
    const zSTRING instanceName = npc->GetInstanceName();
    const zSTRING guildName = npc->GetGuildName();
    const zSTRING bodyState = npc->GetBodyStateName();
    const zSTRING aiState = npc->state.GetStateInfo();
    const zSTRING routine = npc->state.GetRoutineName();
    json result = {
        {"ok", true}, {"name", vobNameUtf8(npc)}, {"instance", transcode(instanceName.ToChar(), 1250, CP_UTF8)},
        {"position", vec(position)}, {"orientation", json{{"right", vec(npc->GetRightVectorWorld())},
            {"up", vec(npc->GetUpVectorWorld())}, {"at", vec(npc->GetAtVectorWorld())}}},
        {"matrix", matrixJson(npc->trafoObjToWorld)},
        {"health", json{{"current", npc->GetAttribute(NPC_ATR_HITPOINTS)},
                          {"maximum", npc->GetAttribute(NPC_ATR_HITPOINTSMAX)}}},
        {"life", json{{"dead", npc->IsDead() != 0}, {"unconscious", npc->IsUnconscious() != 0}}},
        {"guild", json{{"id", npc->GetGuild()}, {"name", transcode(guildName.ToChar(), 1250, CP_UTF8)}}},
        {"ai", json{{"state_id", npc->GetAIState()}, {"state", transcode(aiState.ToChar(), 1250, CP_UTF8)},
                     {"routine", transcode(routine.ToChar(), 1250, CP_UTF8)},
                     {"routine_active", npc->state.IsInRoutine() != 0},
                     {"current_action", transcode(npc->state.curState.name.ToChar(), 1250, CP_UTF8)},
                     {"action_phase", npc->state.curState.phase},
                     {"action_valid", npc->state.curState.valid != 0},
                     {"action_elapsed", std::isfinite(npc->state.curState.stateTime) ?
                         json(npc->state.curState.stateTime) : json(nullptr)},
                     {"next_action", transcode(npc->state.nextState.name.ToChar(), 1250, CP_UTF8)}}},
        {"waypoint_name", transcode(npc->wpname.ToChar(), 1250, CP_UTF8)},
        {"combat", json{{"enemy", vobReference(npc->enemy)}, {"focus", vobReference(npc->focus_vob)},
                         {"fight_move_id", npc->GetCurrentFightMove()}, {"defending", npc->isDefending != 0}}},
        {"animation", json{{"body_state", transcode(bodyState.ToChar(), 1250, CP_UTF8)},
                            {"mds", transcode(npc->mds_name.ToChar(), 1250, CP_UTF8)},
                            {"body_visual", transcode(npc->body_visualName.ToChar(), 1250, CP_UTF8)},
                            {"head_visual", transcode(npc->head_visualName.ToChar(), 1250, CP_UTF8)},
                            {"controller_available", npc->GetAnictrl() != nullptr}}},
        {"dialogue", json{{"active", npc->talkOther != nullptr}, {"talking_to", vobReference(npc->talkOther)},
                           {"can_talk_value", std::isfinite(npc->canTalk) ? json(npc->canTalk) : json(nullptr)},
                           {"info_available", npc->info != nullptr}}},
        {"inventory", json{{"packed_item_count", std::max(0, npc->inventory2.GetNumItemsInPackString())},
                            {"packed_items", json::array()},
                            {"equipped_weapon", nullptr}}}
    };
    const int packedCount = std::max(0, npc->inventory2.GetNumItemsInPackString());
    for (int slot = 0; slot < std::min(inventoryLimit, packedCount); ++slot) {
        zSTRING packedName;
        const int amount = npc->inventory2.GetPackedItemBySlot(slot, packedName);
        result["inventory"]["packed_items"].push_back(json{
            {"slot", slot}, {"name", transcode(packedName.ToChar(), 1250, CP_UTF8)}, {"amount", amount}});
    }
    result["inventory"]["packed_items_truncated"] = packedCount > inventoryLimit;
    if (oCItem* weapon = npc->GetWeapon()) {
        const zSTRING weaponInstance = weapon->GetInstanceName();
        result["inventory"]["equipped_weapon"] = json{{"name", vobNameUtf8(weapon)},
            {"instance", transcode(weaponInstance.ToChar(), 1250, CP_UTF8)}, {"amount", weapon->amount}};
    }
    if (npc->state.aiStateDriven) result["ai"]["target_position"] = vec(npc->state.aiStatePosition);
    if (npc->state.parOther) result["ai"]["other_npc"] = vobReference(npc->state.parOther);
    if (npc->state.parVictim) result["ai"]["victim_npc"] = vobReference(npc->state.parVictim);
    if (npc->state.parItem) result["ai"]["item"] = vobReference(npc->state.parItem);
    if (world && world->wayNet) {
        zCWaypoint* nearest = world->wayNet->GetNearestWaypoint(position);
        if (nearest) {
            const zVEC3 waypointPosition = nearest->GetPositionWorld();
            const zSTRING waypointName = nearest->GetName();
            result["nearest_waypoint"] = json{{"name", transcode(waypointName.ToChar(), 1250, CP_UTF8)},
                {"position", vec(waypointPosition)}, {"distance", distanceBetween(position, waypointPosition)}};
        }
    }
    return result;
}

json npcInspect(const json& request, zCWorld* world) {
    if (!world) fail("world_not_ready", "no world is currently loaded");
    if (!request.contains("name") || !request["name"].is_string())
        fail("invalid_request", "npc_inspect requires a VOb name");
    zCVob* vob = findVobByName(world, request["name"].get<std::string>());
    if (!vob) fail("vob_not_found", "VOb was not found");
    if (className(vob).find("oCNpc") == std::string::npos)
        fail("object_not_npc", "the named VOb is not an NPC");
    const int inventoryLimit = boundedRequestInt(request, "inventory_limit", 8, 0, 16);
    return inspectNpc(static_cast<oCNpc*>(vob), world, inventoryLimit);
}

json npcSearch(const json& request, zCWorld* world) {
    if (!world) fail("world_not_ready", "no world is currently loaded");
    const std::string nameFilter = request.value("name_contains", std::string{});
    const std::string instanceFilter = request.value("instance_contains", std::string{});
    const std::string guildFilter = request.value("guild_contains", std::string{});
    for (const std::string* filter : {&nameFilter, &instanceFilter, &guildFilter})
        if (filter->size() > 128 || filter->find('\0') != std::string::npos)
            fail("invalid_request", "NPC search filters must be at most 128 characters");
    const int limit = boundedRequestInt(request, "limit", 25, 1, 100);
    const int offset = boundedRequestInt(request, "offset", 0, 0, 100000);
    const int scanLimit = boundedRequestInt(request, "scan_limit", 20000, 1, 200000);
    const bool hasCenter = request.contains("center") || request.contains("center_vob");
    if (request.contains("center") && request.contains("center_vob"))
        fail("invalid_request", "provide center or center_vob, not both");
    zVEC3 center(0, 0, 0);
    if (request.contains("center")) center = readVec(request.at("center"));
    if (request.contains("center_vob")) {
        if (!request["center_vob"].is_string()) fail("invalid_request", "center_vob must be a string");
        zCVob* centerVob = findVobByName(world, request["center_vob"].get<std::string>());
        if (!centerVob) fail("vob_not_found", "center VOb was not found");
        center = vobPosition(centerVob);
    }
    float radius = 0.0f;
    if (request.contains("radius")) {
        radius = request.at("radius").get<float>();
        if (!hasCenter || !std::isfinite(radius) || radius < 0.0f || radius > 100000.0f)
            fail("invalid_request", "radius must be 0..100000 and requires center or center_vob");
    }
    zCArray<zCVob*> candidates;
    if (!oCNpc::classDef) fail("npc_search_unavailable", "NPC class metadata is unavailable");
    world->SearchVobListByBaseClass(oCNpc::classDef, candidates, &world->globalVobTree);
    const int totalNpcs = std::max(0, candidates.numInArray);
    const int effectiveLimit = std::min(scanLimit, totalNpcs);
    struct Match { oCNpc* npc; float distance; };
    std::vector<Match> matches;
    int scanned = 0;
    const auto scanStart = std::chrono::steady_clock::now();
    bool timeLimited = false;
    while (scanned < effectiveLimit) {
        if ((scanned & 63) == 0) {
            checkRequestControl(requestState.id, requestState.deadlineMs);
            if (std::chrono::steady_clock::now() - scanStart >= std::chrono::milliseconds(15)) {
                timeLimited = true;
                break;
            }
        }
        zCVob* vob = candidates[scanned++];
        if (vob) {
            auto* npc = static_cast<oCNpc*>(vob);
            const std::string objectName = vobNameUtf8(npc);
            const zSTRING instance = npc->GetInstanceName();
            const zSTRING guild = npc->GetGuildName();
            const std::string instanceText = transcode(instance.ToChar(), 1250, CP_UTF8);
            const std::string guildText = transcode(guild.ToChar(), 1250, CP_UTF8);
            const float distance = hasCenter ? distanceBetween(vobPosition(npc), center) : 0.0f;
            if ((nameFilter.empty() || objectName.find(nameFilter) != std::string::npos) &&
                (instanceFilter.empty() || instanceText.find(instanceFilter) != std::string::npos) &&
                (guildFilter.empty() || guildText.find(guildFilter) != std::string::npos) &&
                (!request.contains("radius") || distance <= radius))
                matches.push_back(Match{npc, distance});
        }
    }
    if (hasCenter) std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        return a.distance < b.distance;
    });
    json rows = json::array();
    const size_t begin = std::min(matches.size(), static_cast<size_t>(offset));
    const size_t end = std::min(matches.size(), begin + static_cast<size_t>(limit));
    for (size_t i = begin; i < end; ++i) {
        oCNpc* npc = matches[i].npc;
        const zSTRING instance = npc->GetInstanceName();
        const zSTRING guild = npc->GetGuildName();
        json row = {{"name", vobNameUtf8(npc)}, {"class", className(npc)},
            {"instance", transcode(instance.ToChar(), 1250, CP_UTF8)},
            {"position", vec(vobPosition(npc))}, {"health", npc->GetAttribute(NPC_ATR_HITPOINTS)},
            {"health_max", npc->GetAttribute(NPC_ATR_HITPOINTSMAX)},
            {"dead", npc->IsDead() != 0}, {"guild", transcode(guild.ToChar(), 1250, CP_UTF8)}};
        if (hasCenter) row["distance"] = matches[i].distance;
        rows.push_back(std::move(row));
    }
    return json{{"ok", true}, {"results", std::move(rows)}, {"matches_scanned", matches.size()},
        {"nodes_scanned", scanned}, {"total_npcs", totalNpcs}, {"scan_truncated", scanned < totalNpcs}, {"scan_time_limited", timeLimited},
        {"offset", offset}, {"limit", limit}};
}

json playerState(zCWorld* world) {
    oCNpc* player = ogame ? ogame->GetSelfPlayerVob() : oCNpc::player;
    if (!player) fail("player_unavailable", "player NPC is unavailable");
    json result = inspectNpc(player, world);
    result["player"] = true;
    result["movement"] = nullptr;
    if (player->human_ai) {
        result["movement"] = json{{"state_id", static_cast<int>(player->human_ai->state)},
            {"velocity", vec(player->human_ai->velocity)},
            {"velocity_length_squared", player->human_ai->velocityLen2},
            {"feet_y", player->human_ai->feetY}, {"head_y", player->human_ai->headY},
            {"above_floor", player->human_ai->aboveFloor}, {"water_level", player->human_ai->waterLevel},
            {"fall_distance_y", player->human_ai->fallDownDistanceY},
            {"fall_start_y", player->human_ai->fallDownStartY},
            {"slide_normal", vec(player->human_ai->slidePolyNormal)}};
    }
    result["collision_support"] = characterSupport(vobPosition(player), player);
    if (player->human_ai) {
        const zVEC3 slideNormal = player->human_ai->slidePolyNormal;
        const float normalLength = std::sqrt(dotVec(slideNormal, slideNormal));
        result["movement"]["slide_slope_degrees"] = normalLength > 0.001f ?
            json(std::acos(std::clamp(std::abs(slideNormal.n[1] / normalLength), 0.0f, 1.0f)) *
                 (180.0f / 3.14159265358979323846f)) : json(nullptr);
    }
    if (world) result["ground_probe"] = rayResult(world,
        zVEC3(vobPosition(player).n[0], vobPosition(player).n[1] + 150.0f, vobPosition(player).n[2]),
        zVEC3(0, -500, 0), zTRACERAY_STAT_POLY | zTRACERAY_POLY_2SIDED | zTRACERAY_POLY_NORMAL);
    return result;
}

json playerCollision(zCWorld* world) {
    if (!world) fail("world_not_ready", "no world is currently loaded");
    oCNpc* player = ogame ? ogame->GetSelfPlayerVob() : oCNpc::player;
    if (!player) fail("player_unavailable", "player NPC is unavailable");
    const zVEC3 position = vobPosition(player);
    const zVEC3 at = player->GetAtVectorWorld();
    const zVEC3 right = player->GetRightVectorWorld();
    const zVEC3 origin(position.n[0], position.n[1] + 60.0f, position.n[2]);
    const int obstacleFlags = zTRACERAY_VOB_BBOX | zTRACERAY_STAT_POLY |
        zTRACERAY_POLY_2SIDED | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_NORMAL;
    json probes = json::array();
    probes.push_back(json{{"direction", "forward"}, {"ray", rayResult(world, origin,
        zVEC3(at.n[0] * 180.0f, at.n[1] * 180.0f, at.n[2] * 180.0f), obstacleFlags)}});
    probes.push_back(json{{"direction", "backward"}, {"ray", rayResult(world, origin,
        zVEC3(-at.n[0] * 180.0f, -at.n[1] * 180.0f, -at.n[2] * 180.0f), obstacleFlags)}});
    probes.push_back(json{{"direction", "right"}, {"ray", rayResult(world, origin,
        zVEC3(right.n[0] * 140.0f, right.n[1] * 140.0f, right.n[2] * 140.0f), obstacleFlags)}});
    probes.push_back(json{{"direction", "left"}, {"ray", rayResult(world, origin,
        zVEC3(-right.n[0] * 140.0f, -right.n[1] * 140.0f, -right.n[2] * 140.0f), obstacleFlags)}});

    json result = { {"ok", true}, {"position", vec(position)},
        {"character_support", characterSupport(position, player)}, {"obstacle_probes", std::move(probes)},
        {"probe_origin", vec(origin)}, {"probe_height_above_origin", 60.0f},
        {"probe_geometry", "world static polygons and VOb bounding boxes, excluding characters"},
        {"full_character_simulation", false} };
    if (player->human_ai) {
        zCAIPlayer* motion = player->human_ai;
        const zVEC3 slideNormal = motion->slidePolyNormal;
        const float normalLength = std::sqrt(dotVec(slideNormal, slideNormal));
        result["movement"] = json{{"state_id", static_cast<int>(motion->state)},
            {"velocity", vec(motion->velocity)}, {"feet_y", motion->feetY}, {"head_y", motion->headY},
            {"above_floor", motion->aboveFloor}, {"fall_distance_y", motion->fallDownDistanceY},
            {"fall_start_y", motion->fallDownStartY}, {"slide_normal", vec(slideNormal)},
            {"slide_slope_degrees", normalLength > 0.001f ?
                json(std::acos(std::clamp(std::abs(slideNormal.n[1] / normalLength), 0.0f, 1.0f)) *
                     (180.0f / 3.14159265358979323846f)) : json(nullptr)}};
    } else result["movement"] = nullptr;
    return result;
}

void restorePlayerInput() {
    if (pendingPlayerControl.inputStateSaved && zinput)
        zinput->SetState(pendingPlayerControl.inputKey, pendingPlayerControl.savedInputState);
    pendingPlayerControl.inputStateSaved = false;
}

void finishPlayerControl(bool success, const std::string& reason) {
    const PendingPlayerControl finished = pendingPlayerControl;
    restorePlayerInput();
    pendingPlayerControl = PendingPlayerControl{};
    zCWorld* world = currentWorld();
    oCNpc* player = ogame ? ogame->GetSelfPlayerVob() : oCNpc::player;
    const zVEC3 finalPosition = player ? vobPosition(player) : finished.startPosition;
    const float moved = distanceBetween(finalPosition, finished.startPosition);
    json response = {{"ok", success}, {"reason", reason}, {"start_position", vec(finished.startPosition)},
        {"end_position", vec(finalPosition)}, {"distance_moved", moved},
        {"elapsed_ms", unixTimeMs() - finished.startMs}, {"input_released", true},
        {"control_ai_frames", finished.controlAiFrames},
        {"world_matched", world == finished.world}, {"player_available", player != nullptr}};
    if (finished.walkTo) {
        response["target_position"] = vec(finished.targetPosition);
        response["target_distance"] = player ? json(distanceBetween(finalPosition, finished.targetPosition)) : json(nullptr);
        response["target_reached"] = player && distanceBetween(finalPosition, finished.targetPosition) <= finished.tolerance;
    }
    if (success) queueResponse(std::move(response), finished.id, finished.command);
    else {
        response["error_code"] = reason;
        response["error"] = reason == "movement_stalled" ? "player position stopped making progress" :
            reason == "cancelled" ? "movement cancelled by client" :
            reason == "world_changed" ? "loaded world changed during player control" :
            reason == "player_unavailable" ? "player became unavailable or dead" :
            reason == "movement_timeout" ? "movement duration expired before reaching its target" :
            "player movement operation failed";
        queueResponse(std::move(response), finished.id, finished.command);
        writeDiagnostic(reason, finished.id, finished.command);
    }
}

void beginPlayerControl(const json& request, const std::string& id, const char* command,
                        zCWorld* world, bool walkTo) {
    const BridgeSettings& settings = bridgeSettings();
    if (!settings.configValid || !settings.allowPlayerControl)
        fail("capability_disabled", "player control is disabled in local configuration");
    if (pendingPlayerControl.active) fail("operation_busy", "another player control operation is active");
    if (!world || !ogame) fail("world_not_ready", "no world is currently loaded");
    oCNpc* player = ogame->GetSelfPlayerVob();
    if (!player) fail("player_unavailable", "player NPC is unavailable");
    if (player->IsDead() || player->IsUnconscious()) fail("player_unavailable", "player is dead or unconscious");
    if (!zinput) fail("input_unavailable", "game input system is unavailable");

    PendingPlayerControl operation;
    operation.active = true;
    operation.walkTo = walkTo;
    operation.id = id;
    operation.command = command;
    operation.world = world;
    operation.player = player;
    operation.startMs = unixTimeMs();
    operation.requestDeadlineMs = requestState.deadlineMs;
    operation.startPosition = vobPosition(player);
    const int duration = boundedRequestInt(request, "duration_ms", walkTo ? 5000 : 1000, 100, 30000);
    operation.endMs = operation.startMs + static_cast<std::uint64_t>(duration);
    operation.lastProgressMs = operation.startMs;

    if (walkTo) {
        if (request.contains("target") && request.contains("waypoint"))
            fail("invalid_request", "provide target or waypoint, not both");
        if (request.contains("target")) operation.targetPosition = readVec(request.at("target"));
        else if (request.contains("waypoint") && request["waypoint"].is_string() && world->wayNet) {
            const std::string waypointName = request["waypoint"].get<std::string>();
            if (waypointName.empty() || waypointName.size() > 128 || waypointName.find('\0') != std::string::npos)
                fail("invalid_request", "waypoint name must contain 1..128 characters");
            const std::string encoded = transcode(waypointName, CP_UTF8, 1250);
            zCWaypoint* waypoint = world->wayNet->GetWaypoint(zSTRING(encoded.c_str()));
            if (!waypoint) fail("waypoint_not_found", "walk target waypoint was not found");
            operation.targetPosition = waypoint->GetPositionWorld();
        } else fail("invalid_request", "player_walk_to requires target coordinates or waypoint");
        operation.tolerance = boundedRequestFloat(request, "tolerance", 100.0f, 20.0f, 1000.0f);
        operation.stallLimitMs = static_cast<std::uint64_t>(boundedRequestInt(request, "stagnation_ms", 2500, 500, 10000));
        operation.lastDistance = distanceBetween(operation.startPosition, operation.targetPosition);
        checkRequestControl(id, requestState.deadlineMs);
        zVEC3 unitHeading;
        if (unitHorizontalDirection(
                zVEC3(operation.targetPosition.n[0] - operation.startPosition.n[0], 0.0f,
                      operation.targetPosition.n[2] - operation.startPosition.n[2]), unitHeading)) {
            player->SetHeadingAtWorld(unitHeading);
        }
        operation.inputKey = GAME_UP;
    } else {
        if (!request.contains("direction") || !request["direction"].is_string())
            fail("invalid_request", "player_move requires direction: forward, backward, strafe_left, or strafe_right");
        const std::string direction = request["direction"].get<std::string>();
        if (direction == "forward") operation.inputKey = GAME_UP;
        else if (direction == "backward") operation.inputKey = GAME_DOWN;
        else if (direction == "strafe_left") operation.inputKey = GAME_STRAFELEFT;
        else if (direction == "strafe_right") operation.inputKey = GAME_STRAFERIGHT;
        else fail("invalid_request", "unsupported player movement direction");
    }
    checkRequestControl(id, requestState.deadlineMs);
    operation.savedInputState = zinput->GetState(operation.inputKey) > 0.0f ? 1 : 0;
    operation.inputStateSaved = true;
    pendingPlayerControl = operation;
    zinput->SetState(operation.inputKey, 1);
}

void processPendingPlayerControl() {
    if (!pendingPlayerControl.active) return;
    const std::string id = pendingPlayerControl.id;
    try {
        checkRequestControl(id, pendingPlayerControl.requestDeadlineMs);
        zCWorld* world = currentWorld();
        oCNpc* player = ogame ? ogame->GetSelfPlayerVob() : oCNpc::player;
        if (!world || world != pendingPlayerControl.world) {
            finishPlayerControl(false, "world_changed");
            return;
        }
        if (!player || player != pendingPlayerControl.player || player->IsDead() || player->IsUnconscious()) {
            finishPlayerControl(false, "player_unavailable");
            return;
        }
        const std::uint64_t now = unixTimeMs();
        if (pendingPlayerControl.walkTo) {
            const float distance = distanceBetween(vobPosition(player), pendingPlayerControl.targetPosition);
            if (distance <= pendingPlayerControl.tolerance) {
                finishPlayerControl(true, "target_reached");
                return;
            }
            if (pendingPlayerControl.lastDistance - distance >= 10.0f) {
                pendingPlayerControl.lastDistance = distance;
                pendingPlayerControl.lastProgressMs = now;
            } else if (now - pendingPlayerControl.lastProgressMs >= pendingPlayerControl.stallLimitMs) {
                finishPlayerControl(false, "movement_stalled");
                return;
            }
        }
        if (now >= pendingPlayerControl.endMs) {
            finishPlayerControl(!pendingPlayerControl.walkTo,
                pendingPlayerControl.walkTo ? "movement_timeout" : "duration_elapsed");
            return;
        }
    } catch (const BridgeError& error) {
        const std::string code = error.code;
        finishPlayerControl(false, code == "timeout" ? "movement_timeout" : code);
    } catch (...) {
        finishPlayerControl(false, "movement_failed");
    }
}

json playerTeleport(const json& request, zCWorld* world) {
    const BridgeSettings& settings = bridgeSettings();
    if (!settings.configValid || !settings.allowPlayerControl)
        fail("capability_disabled", "player control is disabled in local configuration");
    if (!world || !ogame) fail("world_not_ready", "no world is currently loaded");
    oCNpc* player = ogame->GetSelfPlayerVob();
    if (!player) fail("player_unavailable", "player NPC is unavailable");
    if (player->IsDead() || player->IsUnconscious()) fail("player_unavailable", "player is dead or unconscious");
    if (request.contains("position") && request.contains("waypoint"))
        fail("invalid_request", "provide position or waypoint, not both");
    zVEC3 target(0, 0, 0);
    if (request.contains("position")) target = readVec(request.at("position"));
    else if (request.contains("waypoint") && request["waypoint"].is_string() && world->wayNet) {
        const std::string name = request["waypoint"].get<std::string>();
        const std::string encoded = transcode(name, CP_UTF8, 1250);
        zCWaypoint* waypoint = world->wayNet->GetWaypoint(zSTRING(encoded.c_str()));
        if (!waypoint) fail("waypoint_not_found", "teleport waypoint was not found");
        target = waypoint->GetPositionWorld();
    } else fail("invalid_request", "player_teleport requires position or waypoint");
    checkRequestControl(requestState.id, requestState.deadlineMs);
    const zVEC3 before = vobPosition(player);
    player->SetPositionWorld(target);
    if (request.contains("look_at")) {
        const zVEC3 lookAt = readVec(request.at("look_at"));
        const zVEC3 heading(lookAt.n[0] - target.n[0], 0.0f, lookAt.n[2] - target.n[2]);
        zVEC3 unitHeading;
        if (unitHorizontalDirection(heading, unitHeading)) player->SetHeadingAtWorld(unitHeading);
    }
    const zVEC3 after = vobPosition(player);
    return json{{"ok", true}, {"before", vec(before)}, {"requested_position", vec(target)},
        {"actual_position", vec(after)}, {"movement_distance", distanceBetween(before, after)},
        {"teleport_api", "zCVob::SetPositionWorld"}, {"collision_revalidation", "next engine movement frame"}};
}

json playerTurn(const json& request, zCWorld* world) {
    const BridgeSettings& settings = bridgeSettings();
    if (!settings.configValid || !settings.allowPlayerControl)
        fail("capability_disabled", "player control is disabled in local configuration");
    if (!world || !ogame) fail("world_not_ready", "no world is currently loaded");
    oCNpc* player = ogame->GetSelfPlayerVob();
    if (!player || player->IsDead() || player->IsUnconscious()) fail("player_unavailable", "player NPC is unavailable");
    if (!request.contains("look_at")) fail("invalid_request", "player_turn requires look_at coordinates");
    const zVEC3 position = vobPosition(player);
    const zVEC3 lookAt = readVec(request.at("look_at"));
    const zVEC3 heading(lookAt.n[0] - position.n[0], 0.0f, lookAt.n[2] - position.n[2]);
    zVEC3 unitHeading;
    if (!unitHorizontalDirection(heading, unitHeading))
        fail("invalid_request", "look_at must differ horizontally from player position");
    checkRequestControl(requestState.id, requestState.deadlineMs);
    player->SetHeadingAtWorld(unitHeading);
    return json{{"ok", true}, {"position", vec(position)}, {"look_at", vec(lookAt)},
        {"heading", vec(player->GetAtVectorWorld())}};
}

void beginScreenshot(const std::string& id) {
    if (pendingScreenshot.active) fail("operation_busy", "a screenshot is already pending");
    pendingScreenshot.active = true;
    pendingScreenshot.id = id;
    pendingScreenshot.filename = "devbridge-" + id + ".tga";
}

void capturePendingScreenshot() {
    if (!pendingScreenshot.active) return;
    const std::string id = pendingScreenshot.id;
    const std::string filename = pendingScreenshot.filename;
    pendingScreenshot = PendingScreenshot{};
    try {
        checkRequestControl(id, requestState.deadlineMs);
        if (!zrenderer) fail("renderer_unavailable", "renderer is unavailable");
        const auto dir = exchangeDirectory();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) fail("screenshot_directory_unavailable", "cannot create screenshot output directory");
        const auto path = dir / std::filesystem::path(filename);
        if (std::filesystem::exists(path, ec)) fail("screenshot_exists", "screenshot path already exists");
        std::unique_ptr<zCTextureConvert> image(zrenderer->CreateTextureConvert());
        if (!image) fail("screenshot_capture_failed", "renderer could not create a texture conversion buffer");
        if (!zrenderer->Vid_GetFrontBufferCopy(*image)) fail("screenshot_capture_failed", "renderer could not copy the front buffer");
        zTRnd_TextureFormat outputFormat = zRND_TEX_FORMAT_BGRA_8888;
        image->ConvertToNewFormat(outputFormat);
        const zCTextureInfo textureInfo = image->GetTextureInfo();
        if (textureInfo.sizeX <= 0 || textureInfo.sizeY <= 0 ||
            textureInfo.sizeX > 65535 || textureInfo.sizeY > 65535 ||
            textureInfo.texFormat != zRND_TEX_FORMAT_BGRA_8888)
            fail("screenshot_capture_failed", "renderer returned an unsupported screenshot buffer");
        const std::size_t pixelBytes = static_cast<std::size_t>(textureInfo.sizeX) *
            static_cast<std::size_t>(textureInfo.sizeY) * 4;
        if (pixelBytes > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            fail("screenshot_capture_failed", "screenshot buffer exceeds the supported size");

        std::vector<unsigned char> pixels(pixelBytes);
        const int rowPitch = textureInfo.sizeX * 4;
        if (!image->CopyTextureDataTo(0, pixels.data(), rowPitch))
            fail("screenshot_capture_failed", "renderer could not copy screenshot pixels");

        unsigned char header[18]{};
        header[2] = 2; // Uncompressed true-color TGA.
        header[12] = static_cast<unsigned char>(textureInfo.sizeX & 0xff);
        header[13] = static_cast<unsigned char>((textureInfo.sizeX >> 8) & 0xff);
        header[14] = static_cast<unsigned char>(textureInfo.sizeY & 0xff);
        header[15] = static_cast<unsigned char>((textureInfo.sizeY >> 8) & 0xff);
        header[16] = 32;
        header[17] = 0x28; // Eight alpha bits, top-left image origin.
        std::ofstream output(path, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!output) fail("screenshot_save_failed", "screenshot output file could not be opened");
        output.write(reinterpret_cast<const char*>(header), sizeof(header));
        output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixelBytes));
        output.flush();
        if (!output) {
            output.close();
            std::filesystem::remove(path, ec);
            fail("screenshot_save_failed", "screenshot image data could not be written");
        }
        output.close();
        ec.clear();
        const auto bytes = std::filesystem::file_size(path, ec);
        if (ec || bytes == 0) fail("screenshot_save_failed", "screenshot file is missing or empty");
        json response = {{"ok", true}, {"filename", filename}, {"path", path.string()},
            {"format", "TGA"}, {"bytes", bytes}, {"capture_stage", "after_render_blit"}};
        queueResponse(std::move(response), id, "screenshot");
    } catch (const BridgeError& error) {
        queueError(id, "screenshot", error.code, error.what());
    } catch (const std::exception& error) {
        queueError(id, "screenshot", "screenshot_failed", safeErrorText(error.what()));
    } catch (...) {
        queueError(id, "screenshot", "screenshot_failed", "unexpected screenshot error");
    }
}

void finishMonitor(bool success, const std::string& reason = {}) {
    const PendingMonitor finished = pendingMonitor;
    pendingMonitor = PendingMonitor{};
    if (!success) {
        queueError(finished.id, "monitor", reason.empty() ? "monitor_failed" : reason,
                   reason == "world_changed" ? "loaded world changed during monitoring" :
                   reason == "target_not_found" ? "monitored target is no longer available" :
                   reason == "cancelled" ? "monitoring cancelled by client" :
                   reason == "timeout" ? "monitoring deadline expired" : "monitoring failed");
        return;
    }
    json response = {{"ok", true}, {"reason", reason}, {"target_type", finished.targetType},
        {"target", finished.targetName.empty() ? json(nullptr) : json(finished.targetName)},
        {"reference_vob", finished.referenceName.empty() ? json(nullptr) : json(finished.referenceName)},
        {"duration_ms", unixTimeMs() - finished.startMs}, {"sample_count", finished.samples.size()},
        {"stalled", finished.stalled}, {"no_progress_detected", finished.stalled},
        {"samples", finished.samples}};
    queueResponse(std::move(response), finished.id, "monitor");
}

void beginMonitor(const json& request, const std::string& id, zCWorld* world) {
    if (!world) fail("world_not_ready", "no world is currently loaded");
    if (pendingMonitor.active) fail("operation_busy", "a monitor request is already active");
    const std::string targetType = request.value("target_type", std::string{});
    if (targetType != "player" && targetType != "npc" && targetType != "mover")
        fail("invalid_request", "target_type must be player, npc, or mover");
    PendingMonitor monitor;
    monitor.active = true;
    monitor.id = id;
    monitor.targetType = targetType;
    monitor.world = world;
    monitor.startMs = unixTimeMs();
    monitor.endMs = monitor.startMs + static_cast<std::uint64_t>(boundedRequestInt(request, "duration_ms", 5000, 100, 30000));
    monitor.intervalMs = boundedRequestInt(request, "interval_ms", 250, 50, 5000);
    monitor.maxSamples = boundedRequestInt(request, "max_samples", 100, 1, 256);
    monitor.stallLimitMs = static_cast<std::uint64_t>(boundedRequestInt(request, "stagnation_ms", 0, 0, 10000));
    monitor.requestDeadlineMs = requestState.deadlineMs;
    monitor.nextSampleMs = monitor.startMs;
    if (targetType == "npc" || targetType == "mover") {
        if (!request.contains("target") || !request["target"].is_string())
            fail("invalid_request", "npc and mover monitoring require target name");
        monitor.targetName = request["target"].get<std::string>();
        if (monitor.targetName.empty() || monitor.targetName.size() > 128)
            fail("invalid_request", "monitor target name must contain 1..128 characters");
        zCVob* target = findVobByName(world, monitor.targetName);
        if (!target) fail("target_not_found", "monitored VOb was not found");
        const std::string type = className(target);
        if (targetType == "npc" && type.find("oCNpc") == std::string::npos)
            fail("object_not_npc", "monitor target is not an NPC");
        if (targetType == "mover" && type.find("zCMover") == std::string::npos)
            fail("object_not_mover", "monitor target is not a mover");
    } else {
        monitor.initialPlayer = ogame ? ogame->GetSelfPlayerVob() : oCNpc::player;
        if (!monitor.initialPlayer) fail("player_unavailable", "player NPC is unavailable");
    }
    if (request.contains("reference_vob")) {
        if (!request["reference_vob"].is_string()) fail("invalid_request", "reference_vob must be a string");
        monitor.referenceName = request["reference_vob"].get<std::string>();
        if (!findVobByName(world, monitor.referenceName)) fail("vob_not_found", "reference VOb was not found");
    }
    monitor.samples.reserve(static_cast<size_t>(monitor.maxSamples));
    pendingMonitor = std::move(monitor);
}

void processPendingMonitor() {
    if (!pendingMonitor.active) return;
    const std::string id = pendingMonitor.id;
    try {
        checkRequestControl(id, pendingMonitor.requestDeadlineMs);
        zCWorld* world = currentWorld();
        if (!world || world != pendingMonitor.world) {
            finishMonitor(false, "world_changed");
            return;
        }
        const std::uint64_t now = unixTimeMs();
        if (now < pendingMonitor.nextSampleMs) return;

        zCVob* targetVob = nullptr;
        oCNpc* npc = nullptr;
        if (pendingMonitor.targetType == "player") {
            npc = ogame ? ogame->GetSelfPlayerVob() : oCNpc::player;
            if (npc != pendingMonitor.initialPlayer) {
                finishMonitor(false, "player_changed");
                return;
            }
            targetVob = npc;
        } else {
            targetVob = findVobByName(world, pendingMonitor.targetName);
            if (targetVob && pendingMonitor.targetType == "npc" &&
                className(targetVob).find("oCNpc") != std::string::npos)
                npc = static_cast<oCNpc*>(targetVob);
        }
        if (!targetVob || (pendingMonitor.targetType == "npc" && !npc) ||
            (pendingMonitor.targetType == "player" && (!npc || npc->IsDead()))) {
            finishMonitor(false, "target_not_found");
            return;
        }
        const zVEC3 position = vobPosition(targetVob);
        json sample = {{"index", pendingMonitor.samples.size()}, {"elapsed_ms", now - pendingMonitor.startMs},
            {"name", vobNameUtf8(targetVob)}, {"class", className(targetVob)}, {"position", vec(position)}};
        if (npc) {
            const zSTRING instance = npc->GetInstanceName();
            const zSTRING aiState = npc->state.GetStateInfo();
            sample["instance"] = transcode(instance.ToChar(), 1250, CP_UTF8);
            sample["health"] = npc->GetAttribute(NPC_ATR_HITPOINTS);
            sample["health_max"] = npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
            sample["dead"] = npc->IsDead() != 0;
            sample["ai_state"] = transcode(aiState.ToChar(), 1250, CP_UTF8);
            sample["action"] = transcode(npc->state.curState.name.ToChar(), 1250, CP_UTF8);
            sample["routine"] = transcode(npc->state.GetRoutineName().ToChar(), 1250, CP_UTF8);
            if (npc->human_ai) {
                sample["velocity"] = vec(npc->human_ai->velocity);
                sample["fall_distance_y"] = npc->human_ai->fallDownDistanceY;
                sample["above_floor"] = npc->human_ai->aboveFloor;
                sample["ground_contact"] = npc->human_ai->aboveFloor <= 1.0f &&
                    npc->human_ai->fallDownDistanceY < 1.0f;
            }
        }
        if (pendingMonitor.targetType == "mover") {
            auto* mover = static_cast<zCMover*>(targetVob);
            sample["mover_state"] = static_cast<int>(mover->moverState);
            sample["current_keyframe"] = mover->actKeyframe;
            sample["next_keyframe"] = mover->nextKeyframe;
            sample["keyframe_fraction"] = mover->actKeyframeF;
        }
        if (!pendingMonitor.referenceName.empty()) {
            zCVob* reference = findVobByName(world, pendingMonitor.referenceName);
            if (!reference) {
                finishMonitor(false, "reference_not_found");
                return;
            }
            sample["reference_distance"] = distanceBetween(position, vobPosition(reference));
        }
        if (pendingMonitor.stallLimitMs > 0) {
            if (pendingMonitor.hasPosition && distanceBetween(position, pendingMonitor.lastPosition) >= 10.0f)
                pendingMonitor.lastProgressMs = now;
            else if (!pendingMonitor.hasPosition) pendingMonitor.lastProgressMs = now;
            pendingMonitor.stalled = pendingMonitor.hasPosition &&
                now - pendingMonitor.lastProgressMs >= pendingMonitor.stallLimitMs;
        }
        pendingMonitor.lastPosition = position;
        pendingMonitor.hasPosition = true;
        pendingMonitor.samples.push_back(std::move(sample));
        pendingMonitor.nextSampleMs = now + static_cast<std::uint64_t>(pendingMonitor.intervalMs);

        if (pendingMonitor.stalled || now >= pendingMonitor.endMs ||
            pendingMonitor.samples.size() >= static_cast<size_t>(pendingMonitor.maxSamples))
            finishMonitor(true, pendingMonitor.stalled ? "no_progress" :
                pendingMonitor.samples.size() >= static_cast<size_t>(pendingMonitor.maxSamples) ? "sample_limit" : "completed");
    } catch (const BridgeError& error) {
        finishMonitor(false, error.code);
    } catch (...) {
        finishMonitor(false, "monitor_failed");
    }
}

std::vector<zCWaypoint*> waypointsInWorld(zCWayNet* net) {
    std::vector<zCWaypoint*> result;
    if (!net) return result;
    for (zCListSort<zCWaypoint>* item = net->wplist.next; item; item = item->next) {
        if (item->data) result.push_back(item->data);
        if (result.size() >= 100000) break;
    }
    return result;
}

json waypointSummary(zCWaypoint* waypoint) {
    if (!waypoint) return nullptr;
    const zSTRING name = waypoint->GetName();
    return json{{"name", transcode(name.ToChar(), 1250, CP_UTF8)},
                {"position", vec(waypoint->GetPositionWorld())},
                {"water_depth", waypoint->waterDepth}, {"under_water", waypoint->underWater != 0}};
}

json waypointDetail(zCWaypoint* waypoint) {
    if (!waypoint) fail("waypoint_not_found", "waypoint was not found");
    json edges = json::array();
    int edgeCount = 0;
    for (zCList<zCWay>* item = waypoint->wayList.next; item && edgeCount < 512; item = item->next) {
        zCWay* way = item->data;
        if (!way) continue;
        zCWaypoint* goal = way->GetGoalWaypoint(waypoint);
        json edge = {{"target", waypointSummary(goal)}, {"cost", way->cost},
            {"length", way->GetLength()}, {"chasm", way->chasm != 0},
            {"chasm_depth", way->chasmDepth}, {"jump", way->jump != 0}};
        edges.push_back(std::move(edge));
        ++edgeCount;
    }
    json result = waypointSummary(waypoint);
    result["connections"] = std::move(edges);
    result["connection_count"] = edgeCount;
    result["connections_truncated"] = edgeCount >= 512;
    return result;
}

json waypointSearch(const json& request, zCWorld* world) {
    if (!world || !world->wayNet) fail("waynet_unavailable", "WayNet is unavailable");
    const std::string contains = request.value("name_contains", std::string{});
    const std::string exact = request.value("name", std::string{});
    if (contains.size() > 128 || contains.find('\0') != std::string::npos)
        fail("invalid_request", "name_contains must be at most 128 characters");
    if (exact.size() > 128 || exact.find('\0') != std::string::npos)
        fail("invalid_request", "name must be at most 128 characters");
    const int limit = boundedRequestInt(request, "limit", 64, 1, 256);
    const int offset = boundedRequestInt(request, "offset", 0, 0, 100000);
    const std::string containsEncoded = transcode(contains, CP_UTF8, 1250);
    const std::string exactEncoded = transcode(exact, CP_UTF8, 1250);
    json rows = json::array();
    int matched = 0;
    const auto waypoints = waypointsInWorld(world->wayNet);
    for (zCWaypoint* waypoint : waypoints) {
        const zSTRING name = waypoint->GetName();
        if ((!containsEncoded.empty() && std::string(name.ToChar()).find(containsEncoded) == std::string::npos) ||
            (!exactEncoded.empty() && std::string(name.ToChar()) != exactEncoded)) continue;
        if (matched >= offset && rows.size() < static_cast<size_t>(limit)) rows.push_back(waypointDetail(waypoint));
        ++matched;
    }
    return json{{"ok", true}, {"results", std::move(rows)}, {"total_matches", matched},
                {"offset", offset}, {"limit", limit}, {"waypoint_count", waypoints.size()}};
}

json waypointInspect(const json& request, zCWorld* world) {
    if (!world || !world->wayNet) fail("waynet_unavailable", "WayNet is unavailable");
    if (!request.contains("name") || !request["name"].is_string())
        fail("invalid_request", "waypoint_inspect requires a waypoint name");
    const std::string name = request["name"].get<std::string>();
    if (name.empty() || name.size() > 128 || name.find('\0') != std::string::npos)
        fail("invalid_request", "waypoint name must contain 1..128 characters");
    const std::string encoded = transcode(name, CP_UTF8, 1250);
    zCWaypoint* waypoint = world->wayNet->GetWaypoint(zSTRING(encoded.c_str()));
    if (!waypoint) fail("waypoint_not_found", "waypoint was not found");
    json result = waypointDetail(waypoint);
    result["ok"] = true;
    return result;
}

json waynetLocalGraph(const json& request, zCWorld* world) {
    if (!world || !world->wayNet) fail("waynet_unavailable", "WayNet is unavailable");
    if (!request.contains("center") || !request["center"].is_string())
        fail("invalid_request", "waynet_local_graph requires a center waypoint name");
    const std::string centerName = request["center"].get<std::string>();
    if (centerName.empty() || centerName.size() > 128 || centerName.find('\0') != std::string::npos)
        fail("invalid_request", "center waypoint name must contain 1..128 characters");
    const std::string encoded = transcode(centerName, CP_UTF8, 1250);
    zCWaypoint* center = world->wayNet->GetWaypoint(zSTRING(encoded.c_str()));
    if (!center) fail("waypoint_not_found", "center waypoint was not found");
    const int depthLimit = boundedRequestInt(request, "depth", 2, 0, 8);
    const int nodeLimit = boundedRequestInt(request, "max_nodes", 64, 1, 256);
    struct Node { zCWaypoint* waypoint; int depth; };
    std::queue<Node> queue;
    std::unordered_set<zCWaypoint*> visited;
    std::vector<json> rows;
    queue.push(Node{center, 0});
    visited.insert(center);
    bool truncated = false;
    while (!queue.empty()) {
        checkRequestControl(requestState.id, requestState.deadlineMs);
        if (static_cast<int>(rows.size()) >= nodeLimit) { truncated = true; break; }
        const Node node = queue.front();
        queue.pop();
        json row = waypointDetail(node.waypoint);
        row["depth"] = node.depth;
        rows.push_back(std::move(row));
        if (node.depth >= depthLimit) continue;
        for (zCList<zCWay>* item = node.waypoint->wayList.next; item; item = item->next) {
            zCWay* way = item->data;
            if (!way) continue;
            zCWaypoint* next = way->GetGoalWaypoint(node.waypoint);
            if (next && visited.insert(next).second) queue.push(Node{next, node.depth + 1});
        }
    }
    const size_t nodeCount = rows.size();
    return json{{"ok", true}, {"center", waypointSummary(center)}, {"depth", depthLimit},
        {"nodes", std::move(rows)}, {"node_count", nodeCount}, {"truncated", truncated},
        {"graph_direction", "WayNet adjacency as exposed by each waypoint"},
        {"physical_walkability_verified", false}};
}

json waynetNearest(const json& request, zCWorld* world) {
    if (!world || !world->wayNet) fail("waynet_unavailable", "WayNet is unavailable");
    zVEC3 position(0, 0, 0);
    if (request.contains("position")) position = readVec(request.at("position"));
    else if (request.contains("vob") && request["vob"].is_string()) {
        zCVob* vob = findVobByName(world, request["vob"].get<std::string>());
        if (!vob) fail("vob_not_found", "VOb was not found");
        position = vobPosition(vob);
    } else fail("invalid_request", "waynet_nearest requires position or vob");
    zCWaypoint* nearest = world->wayNet->GetNearestWaypoint(position);
    if (!nearest) fail("waypoint_not_found", "WayNet has no nearest waypoint");
    json result = waypointDetail(nearest);
    result["ok"] = true;
    result["query_position"] = vec(position);
    result["distance"] = distanceBetween(position, nearest->GetPositionWorld());
    return result;
}

json waynetCheck(const json& request, zCWorld* world) {
    if (!world || !world->wayNet) fail("waynet_unavailable", "WayNet is unavailable");
    const auto waypoints = waypointsInWorld(world->wayNet);
    std::unordered_set<zCWaypoint*> members(waypoints.begin(), waypoints.end());
    std::unordered_map<zCWaypoint*, int> degree;
    for (zCWaypoint* waypoint : waypoints) degree.emplace(waypoint, 0);
    int edgeCount = 0, isolatedCount = 0, danglingCount = 0;
    json isolated = json::array(), dangling = json::array();
    for (zCWaypoint* waypoint : waypoints) {
        for (zCList<zCWay>* item = waypoint->wayList.next; item; item = item->next) {
            if (!item->data) continue;
            ++edgeCount;
            zCWaypoint* goal = item->data->GetGoalWaypoint(waypoint);
            if (!goal || members.find(goal) == members.end()) {
                ++danglingCount;
                if (dangling.size() < 64) dangling.push_back(waypointSummary(waypoint));
            } else {
                ++degree[waypoint];
                ++degree[goal];
            }
        }
    }
    for (zCWaypoint* waypoint : waypoints) if (degree[waypoint] == 0) {
        ++isolatedCount;
        if (isolated.size() < 128) isolated.push_back(waypointSummary(waypoint));
    }
    const int componentLimit = boundedRequestInt(request, "component_scan_limit", 100000, 1, 100000);
    std::unordered_map<zCWaypoint*, std::vector<zCWaypoint*>> adjacency;
    std::unordered_set<zCWaypoint*> visited;
    json components = json::array();
    int componentCount = 0, nonIsolatedComponents = 0;
    const size_t waypointLimit = std::min(waypoints.size(), static_cast<size_t>(componentLimit));
    for (size_t i = 0; i < waypointLimit; ++i) {
        zCWaypoint* waypoint = waypoints[i];
        if (!visited.insert(waypoint).second) continue;
        std::queue<zCWaypoint*> queue;
        queue.push(waypoint);
        int componentSize = 0;
        json samples = json::array();
        while (!queue.empty()) {
            zCWaypoint* current = queue.front(); queue.pop();
            ++componentSize;
            if (samples.size() < 8) samples.push_back(waypointSummary(current));
            for (zCList<zCWay>* item = current->wayList.next; item; item = item->next) {
                zCWay* way = item->data;
                zCWaypoint* next = way ? way->GetGoalWaypoint(current) : nullptr;
                if (next && members.count(next) && visited.insert(next).second) queue.push(next);
            }
        }
        ++componentCount;
        if (componentSize > 1) ++nonIsolatedComponents;
        if (components.size() < 128)
            components.push_back(json{{"size", componentSize}, {"sample_waypoints", std::move(samples)}});
    }
    return json{{"ok", true}, {"waypoint_count", waypoints.size()}, {"directed_edge_count", edgeCount},
        {"isolated_count", isolatedCount}, {"isolated_waypoints", std::move(isolated)},
        {"dangling_edge_count", danglingCount}, {"dangling_from", std::move(dangling)},
        {"isolated_list_truncated", isolatedCount > 128}, {"dangling_list_truncated", danglingCount > 64},
        {"weak_component_count", componentCount}, {"non_isolated_component_count", nonIsolatedComponents},
        {"weak_components", std::move(components)},
        {"components_truncated", waypoints.size() > waypointLimit || componentCount > 128},
        {"component_scan_limit", componentLimit}};
}

json waynetRoute(const json& request, zCWorld* world) {
    if (!world || !world->wayNet) fail("waynet_unavailable", "WayNet is unavailable");
    if (!request.contains("start") || !request["start"].is_string() ||
        !request.contains("target") || !request["target"].is_string())
        fail("invalid_request", "waynet_route requires start and target waypoint names");
    const std::string startName = request["start"].get<std::string>();
    const std::string targetName = request["target"].get<std::string>();
    if (startName.empty() || targetName.empty() || startName.size() > 128 || targetName.size() > 128)
        fail("invalid_request", "waypoint names must contain 1..128 characters");
    const std::string startEncoded = transcode(startName, CP_UTF8, 1250);
    const std::string targetEncoded = transcode(targetName, CP_UTF8, 1250);
    const zSTRING startZ(startEncoded.c_str()), targetZ(targetEncoded.c_str());
    zCWaypoint* start = world->wayNet->GetWaypoint(startZ);
    zCWaypoint* target = world->wayNet->GetWaypoint(targetZ);
    if (!start || !target) fail("waypoint_not_found", "start or target waypoint was not found");
    const auto waypoints = waypointsInWorld(world->wayNet);
    const int limit = boundedRequestInt(request, "max_nodes", static_cast<int>(std::min<size_t>(waypoints.size(), 20000)),
                                         1, 100000);
    std::unordered_set<zCWaypoint*> members(waypoints.begin(), waypoints.end());
    if (!members.count(start) || !members.count(target)) fail("waypoint_not_found", "waypoint is not in the current WayNet");
    struct Entry { double cost; zCWaypoint* waypoint; bool operator>(const Entry& rhs) const { return cost > rhs.cost; } };
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    std::unordered_map<zCWaypoint*, double> costs;
    std::unordered_map<zCWaypoint*, zCWaypoint*> parents;
    costs[start] = 0.0;
    parents[start] = nullptr;
    open.push(Entry{0.0, start});
    int expanded = 0;
    while (!open.empty() && expanded < limit) {
        const Entry current = open.top(); open.pop();
        const auto currentCost = costs.find(current.waypoint);
        if (currentCost == costs.end() || current.cost != currentCost->second) continue;
        ++expanded;
        if (current.waypoint == target) break;
        for (zCList<zCWay>* item = current.waypoint->wayList.next; item; item = item->next) {
            zCWay* way = item->data;
            if (!way) continue;
            zCWaypoint* next = way->GetGoalWaypoint(current.waypoint);
            if (!next || !members.count(next)) continue;
            double weight = way->cost > 0 ? static_cast<double>(way->cost) :
                static_cast<double>(distanceBetween(current.waypoint->GetPositionWorld(), next->GetPositionWorld()));
            if (!std::isfinite(weight) || weight < 0.0) continue;
            const double candidate = current.cost + weight;
            const auto known = costs.find(next);
            if (known == costs.end() || candidate < known->second) {
                costs[next] = candidate;
                parents[next] = current.waypoint;
                open.push(Entry{candidate, next});
            }
        }
    }
    if (parents.find(target) == parents.end())
        return json{{"ok", true}, {"found", false}, {"expanded", expanded},
            {"truncated", !open.empty() && expanded >= limit}, {"algorithm", "Dijkstra over WayNet edges"},
            {"physical_walkability_verified", false}};
    std::vector<zCWaypoint*> route;
    for (zCWaypoint* node = target; node; node = parents[node]) route.push_back(node);
    std::reverse(route.begin(), route.end());
    json steps = json::array();
    for (zCWaypoint* waypoint : route) steps.push_back(waypointSummary(waypoint));
    return json{{"ok", true}, {"found", true}, {"cost", costs[target]}, {"steps", std::move(steps)},
        {"expanded", expanded}, {"truncated", false}, {"algorithm", "Dijkstra over WayNet edges"},
        {"physical_walkability_verified", false}};
}

const char* daedalusTypeName(unsigned int type) {
    switch (type) {
    case zPAR_TYPE_VOID: return "void";
    case zPAR_TYPE_FLOAT: return "float";
    case zPAR_TYPE_INT: return "int";
    case zPAR_TYPE_STRING: return "string";
    case zPAR_TYPE_CLASS: return "class";
    case zPAR_TYPE_FUNC: return "function";
    case zPAR_TYPE_PROTOTYPE: return "prototype";
    case zPAR_TYPE_INSTANCE: return "instance";
    default: return "unknown";
    }
}

json daedalusSymbolInfo(zCPar_Symbol* symbol, int index) {
    if (!symbol) return nullptr;
    const zSTRING name = symbol->name;
    json result = {{"name", transcode(name.ToChar(), 1250, CP_UTF8)}, {"index", index},
        {"type", daedalusTypeName(symbol->type)}, {"type_id", static_cast<int>(symbol->type)},
        {"element_count", static_cast<int>(symbol->ele)},
        {"constant", (symbol->flags & zPAR_FLAG_CONST) != 0},
        {"class_variable", (symbol->flags & zPAR_FLAG_CLASSVAR) != 0},
        {"flags", static_cast<int>(symbol->flags)},
        {"value_read_supported", symbol->type == zPAR_TYPE_INT || symbol->type == zPAR_TYPE_FLOAT || symbol->type == zPAR_TYPE_STRING}};
    if (symbol->parent) {
        const zSTRING parentName = symbol->parent->name;
        result["parent"] = transcode(parentName.ToChar(), 1250, CP_UTF8);
    } else result["parent"] = nullptr;
    return result;
}

json daedalusSearch(const json& request) {
    if (!parser) fail("parser_unavailable", "Daedalus parser is unavailable");
    const std::string contains = request.value("name_contains", std::string{});
    if (contains.size() > 128 || contains.find('\0') != std::string::npos)
        fail("invalid_request", "name_contains must be at most 128 characters");
    const int limit = boundedRequestInt(request, "limit", 64, 1, 256);
    const int offset = boundedRequestInt(request, "offset", 0, 0, 100000);
    const int scanLimit = boundedRequestInt(request, "scan_limit", 20000, 1, 100000);
    json rows = json::array();
    int scanned = 0, matches = 0;
    const auto scanStart = std::chrono::steady_clock::now();
    bool timeLimited = false;
    zCPar_Symbol* symbol = parser->symtab.firstsym;
    for (; symbol && scanned < scanLimit; symbol = symbol->next) {
        if ((scanned & 63) == 0) {
            checkRequestControl(requestState.id, requestState.deadlineMs);
            if (std::chrono::steady_clock::now() - scanStart >= std::chrono::milliseconds(15)) {
                timeLimited = true;
                break;
            }
        }
        ++scanned;
        const zSTRING name = symbol->name;
        const std::string text = transcode(name.ToChar(), 1250, CP_UTF8);
        if (contains.empty() || text.find(contains) != std::string::npos) {
            if (matches >= offset && rows.size() < static_cast<size_t>(limit))
                rows.push_back(daedalusSymbolInfo(symbol, parser->GetIndex(name)));
            ++matches;
        }
    }
    return json{{"ok", true}, {"results", std::move(rows)}, {"matches_found", matches},
        {"symbols_scanned", scanned}, {"scan_limit", scanLimit}, {"offset", offset}, {"limit", limit},
        {"scan_truncated", symbol != nullptr}, {"scan_time_limited", timeLimited}};
}

json daedalusRead(const json& request) {
    if (!parser) fail("parser_unavailable", "Daedalus parser is unavailable");
    if (!request.contains("symbol") || !request["symbol"].is_string())
        fail("invalid_request", "daedalus_read requires a symbol name");
    const std::string name = request["symbol"].get<std::string>();
    if (!validSymbolName(name)) fail("invalid_request", "symbol name must be printable ASCII and at most 128 characters");
    const zSTRING symbolName(name.c_str());
    const int index = parser->GetIndex(symbolName);
    if (index < 0) fail("configured_symbol_missing", "Daedalus symbol was not found");
    zCPar_Symbol* symbol = parser->GetSymbol(index);
    if (!symbol) fail("configured_symbol_unavailable", "Daedalus symbol is unavailable");
    json result = daedalusSymbolInfo(symbol, index);
    result["ok"] = true;
    const int count = boundedRequestInt(request, "count", 1, 1, 256);
    const int start = boundedRequestInt(request, "start_index", 0, 0, 4095);
    json values = json::array();
    const bool supported = symbol->type == zPAR_TYPE_INT || symbol->type == zPAR_TYPE_FLOAT || symbol->type == zPAR_TYPE_STRING;
    const bool instanceScoped = (symbol->flags & zPAR_FLAG_CLASSVAR) != 0;
    if (!supported || instanceScoped) {
        result["value_available"] = false;
        result["value_reason"] = instanceScoped ? "instance_scoped_symbol_requires_an_active_parser_instance" : "symbol_type_is_metadata_only";
        return result;
    }
    if (symbol->ele == 0 || start >= static_cast<int>(symbol->ele))
        fail("invalid_request", "start_index is outside the symbol array");
    const int actualCount = std::min(count, static_cast<int>(symbol->ele) - start);
    for (int offset = 0; offset < actualCount; ++offset) {
        const int element = start + offset;
        if (symbol->type == zPAR_TYPE_INT) {
            int value = 0; symbol->GetValue(value, element); values.push_back(value);
        } else if (symbol->type == zPAR_TYPE_FLOAT) {
            float value = 0.0f; symbol->GetValue(value, element);
            if (!std::isfinite(value)) values.push_back(nullptr); else values.push_back(value);
        } else {
            zSTRING value; symbol->GetValue(value, element);
            std::string text = transcode(value.ToChar(), 1250, CP_UTF8);
            const bool valueTruncated = text.size() > 4096;
            if (valueTruncated) text.resize(4096);
            values.push_back(text);
            if (valueTruncated) result["string_value_truncated"] = true;
        }
    }
    result["value_available"] = true;
    result["start_index"] = start;
    result["values"] = std::move(values);
    result["values_returned"] = actualCount;
    result["truncated"] = actualCount < count;
    return result;
}

json moverInspect(const json& request, zCWorld* world) {
    if (!world) fail("world_not_ready", "no world is currently loaded");
    if (!request.contains("name") || !request["name"].is_string())
        fail("invalid_request", "mover_inspect requires a VOb name");
    zCVob* vob = findVobByName(world, request["name"].get<std::string>());
    if (!vob) fail("vob_not_found", "VOb was not found");
    if (className(vob).find("zCMover") == std::string::npos)
        fail("object_not_mover", "the named VOb is not a mover");
    zCMover* mover = static_cast<zCMover*>(vob);
    const char* state = "unknown";
    switch (mover->moverState) {
    case zCMover::MOVER_STATE_OPEN: state = "open"; break;
    case zCMover::MOVER_STATE_OPENING: state = "opening"; break;
    case zCMover::MOVER_STATE_CLOSED: state = "closed"; break;
    case zCMover::MOVER_STATE_CLOSING: state = "closing"; break;
    }
    json keyframes = json::array();
    const int keyframeCount = std::clamp(mover->keyframeList.numInArray, 0, 64);
    for (int i = 0; i < keyframeCount; ++i)
        keyframes.push_back(json{{"index", i}, {"position", vec(mover->keyframeList[i].pos)}});
    const zSTRING triggerTarget = mover->GetTriggerTarget();
    const std::string triggerTargetUtf8 = transcode(triggerTarget.ToChar(), 1250, CP_UTF8);
    zCVob* relatedTarget = triggerTargetUtf8.empty() ? nullptr : findVobByName(world, triggerTargetUtf8);
    return json{{"ok", true}, {"vob", vobSummary(vob)}, {"state", state},
        {"state_id", static_cast<int>(mover->moverState)}, {"animation_type", static_cast<int>(mover->moverAniType)},
        {"behavior", static_cast<int>(mover->moverBehavior)}, {"touch_behavior", static_cast<int>(mover->touchBehavior)},
        {"position_lerp_type", static_cast<int>(mover->posLerpType)}, {"speed_type", static_cast<int>(mover->speedType)},
        {"current_keyframe", mover->actKeyframe}, {"next_keyframe", mover->nextKeyframe},
        {"keyframe_fraction", mover->actKeyframeF}, {"move_speed", mover->moveSpeed},
        {"move_speed_unit", mover->moveSpeedUnit}, {"advance_direction", mover->advanceDir},
        {"locked", mover->moverLocked != 0}, {"auto_link_enabled", mover->autoLinkEnabled != 0},
        {"auto_rotate", mover->autoRotate != 0}, {"keyframes", std::move(keyframes)},
        {"keyframes_total", std::max(0, mover->keyframeList.numInArray)},
        {"keyframes_truncated", mover->keyframeList.numInArray > keyframeCount},
        {"trigger_target_name", triggerTargetUtf8}, {"related_target", vobReference(relatedTarget)},
        {"trigger_invoked", false}};
}
