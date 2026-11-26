#ifdef null
#undef null
#endif
#include "json.hpp"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifndef DEVBRIDGE_VERSION
#define DEVBRIDGE_VERSION "development"
#endif
#ifndef DEVBRIDGE_PROTOCOL_VERSION
#define DEVBRIDGE_PROTOCOL_VERSION 1
#endif

namespace GOTHIC_NAMESPACE {
namespace {
using json = nlohmann::json;

constexpr int kProtocolVersion = DEVBRIDGE_PROTOCOL_VERSION;
constexpr std::uint64_t kMaximumRequestLifetimeMs = 180000;
constexpr std::uint64_t kLegacyRequestLifetimeMs = 120000;
constexpr wchar_t kDefaultExchangeDirectory[] = L"ZenGinDevBridge";
constexpr wchar_t kConfigurationFile[] = L"ZenGinDevBridge.config.json";

struct BridgeSettings {
    bool loaded = false;
    bool configValid = true;
    bool allowCameraControl = false;
    bool allowScenarioStart = false;
    std::filesystem::path gameRoot;
    std::filesystem::path exchangeDir;
    std::string configError;
    std::string scenarioRequestSymbol;
    json scenarios = json::object();
    json statusSymbols = json::object();
};

BridgeSettings& bridgeSettings() {
    static BridgeSettings value;
    return value;
}

std::uint64_t unixTimeMs() {
    FILETIME fileTime{};
    GetSystemTimeAsFileTime(&fileTime);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = fileTime.dwLowDateTime;
    ticks.HighPart = fileTime.dwHighDateTime;
    constexpr std::uint64_t kWindowsToUnixEpochMs = 11644473600000ULL;
    return ticks.QuadPart / 10000ULL - kWindowsToUnixEpochMs;
}

bool validSymbolName(const std::string& value) {
    if (value.empty() || value.size() > 128) return false;
    for (unsigned char c : value) {
        if (c < 0x20 || c > 0x7e) return false;
    }
    return true;
}

bool validDirectoryName(const std::string& value) {
    if (value.empty() || value.size() > 64 || value == "." || value == ".." ||
        value.back() == '.') return false;
    for (unsigned char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}

void loadBridgeSettings() {
    BridgeSettings& settings = bridgeSettings();
    if (settings.loaded) return;
    settings.loaded = true;

    wchar_t exe[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    if (length == 0 || length >= std::size(exe)) {
        settings.configValid = false;
        settings.configError = "cannot_resolve_game_root";
        return;
    }

    settings.gameRoot = std::filesystem::path(exe).parent_path().parent_path();
    settings.exchangeDir = settings.gameRoot / kDefaultExchangeDirectory;
    const auto configPath = settings.gameRoot / kConfigurationFile;
    std::error_code ec;
    const auto configSize = std::filesystem::file_size(configPath, ec);
    if (ec == std::errc::no_such_file_or_directory) return;
    if (ec) {
        settings.configValid = false;
        settings.configError = "cannot_read_configuration";
        return;
    }
    if (configSize > 16384) {
        settings.configValid = false;
        settings.configError = "configuration_too_large";
        return;
    }

    try {
        std::ifstream input(configPath, std::ios::binary);
        if (!input) throw std::runtime_error("cannot_open_configuration");
        const json config = json::parse(input);
        if (!config.is_object() || config.value("config_version", 1) != 1)
            throw std::runtime_error("unsupported_configuration_version");

        if (config.contains("exchange_directory")) {
            if (!config["exchange_directory"].is_string())
                throw std::runtime_error("exchange_directory_must_be_a_name");
            const std::string name = config["exchange_directory"].get<std::string>();
            if (!validDirectoryName(name))
                throw std::runtime_error("exchange_directory_must_be_a_single_local_folder_name");
            settings.exchangeDir = settings.gameRoot / std::filesystem::path(name);
        }

        settings.allowCameraControl = config.value("allow_camera_control", false);
        settings.allowScenarioStart = config.value("allow_scenario_start", false);
        if (config.contains("scenario_dispatch")) {
            const auto& dispatch = config["scenario_dispatch"];
            if (!dispatch.is_object())
                throw std::runtime_error("scenario_dispatch_must_be_an_object");
            settings.scenarioRequestSymbol = dispatch.value("request_symbol", std::string{});
            settings.scenarios = dispatch.value("scenarios", json::object());
            settings.statusSymbols = dispatch.value("status_symbols", json::object());
            if (!settings.scenarios.is_object() || !settings.statusSymbols.is_object())
                throw std::runtime_error("scenario_maps_must_be_objects");
            if (settings.scenarios.size() > 64 || settings.statusSymbols.size() > 64)
                throw std::runtime_error("scenario_maps_exceed_limit");
            if (!settings.scenarioRequestSymbol.empty() &&
                !validSymbolName(settings.scenarioRequestSymbol))
                throw std::runtime_error("invalid_scenario_request_symbol");
            for (auto it = settings.scenarios.begin(); it != settings.scenarios.end(); ++it) {
                if (it.key().empty() || it.key().size() > 64 || it.key().find('\0') != std::string::npos ||
                    !it.value().is_number_integer() || it.value().get<int>() < 1 ||
                    it.value().get<int>() > 32767)
                    throw std::runtime_error("invalid_scenario_mapping");
            }
            for (auto it = settings.statusSymbols.begin(); it != settings.statusSymbols.end(); ++it) {
                if (it.key().empty() || it.key().size() > 64 || it.key().find('\0') != std::string::npos || !it.value().is_string() ||
                    !validSymbolName(it.value().get<std::string>()))
                    throw std::runtime_error("invalid_status_symbol_mapping");
                if (it.key() == "id" || it.key() == "command" || it.key() == "ok" ||
                    it.key() == "error" || it.key() == "error_code" ||
                    it.key() == "protocol_version" || it.key() == "plugin_version" ||
                    it.key() == "scenario_status")
                    throw std::runtime_error("reserved_status_alias");
            }
        }
    } catch (const std::exception& error) {
        settings.configValid = false;
        settings.allowCameraControl = false;
        settings.allowScenarioStart = false;
        settings.configError = error.what();
    }
}

std::filesystem::path exchangeDirectory() {
    loadBridgeSettings();
    return bridgeSettings().exchangeDir;
}

std::string transcode(const std::string& text, UINT from, UINT to) {
    if (text.empty()) return {};
    const DWORD decodeFlags = from == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const int wideSize = MultiByteToWideChar(from, decodeFlags, text.data(),
                                              static_cast<int>(text.size()), nullptr, 0);
    if (!wideSize) throw std::runtime_error("invalid source encoding");
    std::wstring wide(static_cast<size_t>(wideSize), L'\0');
    if (!MultiByteToWideChar(from, decodeFlags, text.data(), static_cast<int>(text.size()),
                             wide.data(), wideSize))
        throw std::runtime_error("decode failed");

    BOOL usedDefault = FALSE;
    const DWORD encodeFlags = to == CP_UTF8 ? 0 : WC_NO_BEST_FIT_CHARS;
    const int resultSize = WideCharToMultiByte(to, encodeFlags, wide.data(), wideSize,
                                                nullptr, 0, nullptr,
                                                to == CP_UTF8 ? nullptr : &usedDefault);
    if (!resultSize || usedDefault)
        throw std::runtime_error("name is not representable in CP1250");
    std::string result(static_cast<size_t>(resultSize), '\0');
    usedDefault = FALSE;
    if (!WideCharToMultiByte(to, encodeFlags, wide.data(), wideSize, result.data(),
                             resultSize, nullptr, to == CP_UTF8 ? nullptr : &usedDefault)
        || usedDefault)
        throw std::runtime_error("encode failed");
    return result;
}

json vec(const zVEC3& value) {
    return json::array({value.n[0], value.n[1], value.n[2]});
}

zVEC3 readVec(const json& value) {
    if (!value.is_array() || value.size() != 3)
        throw std::runtime_error("expected three numbers");
    float components[3];
    for (int i = 0; i < 3; ++i) {
        components[i] = value.at(i).get<float>();
        if (!std::isfinite(components[i]) || std::abs(components[i]) > 10000000)
            throw std::runtime_error("invalid vector");
    }
    return zVEC3(components[0], components[1], components[2]);
}

constexpr int kBatchLimit = 256;
constexpr int kGridLimit = 121;
constexpr int kRequestLimit = 65536;
constexpr int kBatchPerFrame = 16;
constexpr int kGridPerFrame = 3;

struct RayProbe { zVEC3 start; zVEC3 direction; int flags; };
struct PendingScan {
    bool active = false;
    bool finished = false;
    bool grid = false;
    bool includeBBox = false;
    std::string id;
    std::uint64_t deadlineMs = 0;
    zCWorld* world = nullptr;
    std::vector<RayProbe> probes;
    std::vector<json> rows;
    size_t next = 0;
    int countX = 0;
    int countZ = 0;
    float spacing = 0.0f;
    float radiusX = 0.0f;
    float radiusZ = 0.0f;
};
PendingScan pendingScan;

struct RequestState {
    bool active = false;
    bool responsePending = false;
    std::string id;
    std::string command;
    std::uint64_t deadlineMs = 0;
    json response;
};
RequestState requestState;

struct CameraControl {
    bool overrideActive = false;
    bool savedValid = false;
    bool savedAIValid = false;
    int lastScenarioDispatched = 0;
    int savedAITranslate = 0;
    int savedAIRotate = 0;
    zMAT4 savedTransform;
    zMAT4 savedCameraWorldTransform;
    zMAT4 overrideTransform;
    zCWorld* savedWorld = nullptr;
    zCCamera* savedCamera = nullptr;
    zCAICamera* savedAI = nullptr;
    zCCamera* activeCamera = nullptr;
    zCVob* activeCameraVob = nullptr;
    std::string lastScenarioName;
};
CameraControl cameraControl;

zCWorld* observedWorld = nullptr;
bool observedWorldInitialized = false;

struct BridgeError : std::runtime_error {
    std::string code;
    BridgeError(std::string errorCode, std::string message)
        : std::runtime_error(std::move(message)), code(std::move(errorCode)) {}
};

[[noreturn]] void fail(const char* code, const char* message) {
    throw BridgeError(code, message);
}

std::string safeErrorText(const std::string& text) {
    constexpr size_t kMaximumErrorLength = 256;
    std::string safe = text.substr(0, kMaximumErrorLength);
    for (char& c : safe) {
        if (static_cast<unsigned char>(c) < 0x20) c = ' ';
    }
    return safe;
}

json materialJson(zCMaterial* material) {
    if (!material) return nullptr;
    const zSTRING& name = material->GetName();
    const zSTRING group = material->GetMatGroupString();
    return json{
        {"name", transcode(name.ToChar(), 1250, CP_UTF8)},
        {"group", transcode(group.ToChar(), 1250, CP_UTF8)},
        {"group_id", static_cast<int>(material->matGroup)},
        {"no_coll_det", static_cast<bool>(material->noCollDet)}
    };
}

std::string className(zCVob* vob) {
    if (!vob) return {};
    zCClassDef* def = vob->_GetClassDef();
    return def ? transcode(def->className.ToChar(), 1250, CP_UTF8) : std::string{};
}

json rayResult(zCWorld* world, const zVEC3& start, const zVEC3& ray, int flags) {
    const int hit = world->TraceRayNearestHit(start, ray, static_cast<zCVob*>(nullptr), flags);
    const auto& report = world->traceRayReport;
    const bool found = hit != 0 && report.foundHit != 0;
    const float length = std::sqrt(ray.n[0]*ray.n[0] + ray.n[1]*ray.n[1] + ray.n[2]*ray.n[2]);
    json result = {{"ok",true},{"hit",found},{"flags",flags},{"ray_vector_length",length}};
    if (!found) return result;
    result["position"] = vec(report.foundIntersection);
    result["height"] = report.foundIntersection.n[1];
    result["normal"] = vec(report.foundPolyNormal);
    result["kind"] = report.foundVob ? "vob" : "world";
    result["geometry_type"] = (flags & zTRACERAY_VOB_BBOX) ? "vob_bbox" : "polygon";
    if (report.foundVob) {
        result["vob_name"] = transcode(report.foundVob->GetObjectName().ToChar(),1250,CP_UTF8);
        result["vob_class"] = className(report.foundVob);
        result["cd_static"] = static_cast<bool>(report.foundVob->collDetectionStatic);
        result["cd_dynamic"] = static_cast<bool>(report.foundVob->collDetectionDynamic);
    }
    result["material"] = report.foundPoly ? materialJson(report.foundPoly->material) : json(nullptr);
    return result;
}

json characterSupport(const zVEC3& position) {
    oCNpc* playerNpc = oCNpc::player;
    if (!playerNpc) return json{{"available",false},{"reason","player NPC unavailable"}};
    zCCollisionObject* collision = playerNpc->GetCollisionObject();
    if (!collision) return json{{"available",false},{"reason","player collision object unavailable"}};
    zCCollisionObjectDef* actual = collision->GetCollObjClass();
    if (!actual || actual != zCCollObjectCharacter::S_GetCollObjClass())
        return json{{"available",false},{"reason","player collision object is not zCCollObjectCharacter"},
                    {"collision_class_index",actual ? actual->m_iIndex : -1}};
    auto* character = static_cast<zCCollObjectCharacter*>(collision);
    zCCollObjectCharacter::zTSpatialState state;
    std::memset(&state,0,sizeof(state));
    character->FindFloorWaterCeiling(position,state);
    json result = {
        {"available",true},
        {"method","FindFloorWaterCeiling on live player collision object"},
        {"supported",state.m_poFloorPoly != nullptr && !state.m_bIsUninited},
        {"has_floor_polygon",state.m_poFloorPoly != nullptr},
        {"floor_y",std::isfinite(state.m_fFloorY) && std::abs(state.m_fFloorY)<10000000 ? json(state.m_fFloorY) : json(nullptr)},
        {"water_y",std::isfinite(state.m_fWaterY) && std::abs(state.m_fWaterY)<10000000 ? json(state.m_fWaterY) : json(nullptr)},
        {"ceiling_y",std::isfinite(state.m_fCeilingY) && std::abs(state.m_fCeilingY)<10000000 ? json(state.m_fCeilingY) : json(nullptr)},
        {"floor_is_stair",static_cast<bool>(state.m_bFloorIsStair)},
        {"floor_is_vob",static_cast<bool>(state.m_bFloorIsVob)},
        {"state_uninitialized",static_cast<bool>(state.m_bIsUninited)},
        {"character_collision_state",static_cast<int>(character->m_oConfig.m_eState)},
        {"step_height",character->m_oConfig.m_fStepHeight},
        {"max_ground_angle_walk",character->m_oConfig.m_fMaxGroundAngleWalk},
        {"treat_water_as_solid",static_cast<bool>(character->m_oConfig.m_bTreatWaterAsSolid)}
    };
    if (state.m_poFloorPoly) result["floor_material"] = materialJson(state.m_poFloorPoly->material);
    return result;
}

zVEC3 subVec(const zVEC3& a,const zVEC3& b) { return zVEC3(a.n[0]-b.n[0],a.n[1]-b.n[1],a.n[2]-b.n[2]); }
zVEC3 crossVec(const zVEC3& a,const zVEC3& b) {
    return zVEC3(a.n[1]*b.n[2]-a.n[2]*b.n[1],a.n[2]*b.n[0]-a.n[0]*b.n[2],a.n[0]*b.n[1]-a.n[1]*b.n[0]);
}
float dotVec(const zVEC3& a,const zVEC3& b) { return a.n[0]*b.n[0]+a.n[1]*b.n[1]+a.n[2]*b.n[2]; }
zVEC3 normVec(const zVEC3& v) {
    const float n=std::sqrt(dotVec(v,v));
    if(n<0.0001f) throw std::runtime_error("camera look direction is too short");
    return zVEC3(v.n[0]/n,v.n[1]/n,v.n[2]/n);
}
zMAT4 cameraLookAt(const zVEC3& position,const zVEC3& target) {
    const zVEC3 at=normVec(subVec(target,position));
    zVEC3 worldUp(0.0f,1.0f,0.0f);
    if(std::abs(dotVec(at,worldUp))>0.999f) worldUp=zVEC3(0.0f,0.0f,1.0f);
    const zVEC3 right=normVec(crossVec(worldUp,at));
    const zVEC3 up=normVec(crossVec(at,right));
    zMAT4 m; m.MakeIdentity(); m.SetRightVector(right); m.SetUpVector(up); m.SetAtVector(at); m.SetTranslation(position);
    return m;
}
json matrixJson(const zMAT4& m) {
    json result=json::array();
    for(int r=0;r<4;++r){json row=json::array();for(int c=0;c<4;++c)row.push_back(m.v[r].n[c]);result.push_back(row);}
    return result;
}
void writeDiagnostic(const std::string& code, const std::string& id = {},
                     const std::string& command = {}) noexcept {
    try {
        static DWORD lastWriteTick = 0;
        static std::string lastWriteKey;
        const DWORD now = GetTickCount();
        const std::string key = code + "|" + id;
        if (key == lastWriteKey && static_cast<DWORD>(now - lastWriteTick) < 5000) return;
        lastWriteKey = key;
        lastWriteTick = now;
        const auto dir = exchangeDirectory();
        if (dir.empty()) return;
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) return;
        json entry = {{"time_unix_ms", unixTimeMs()}, {"pid", GetCurrentProcessId()},
                      {"code", code}};
        if (!id.empty()) entry["id"] = id;
        if (!command.empty()) entry["command"] = command;
        const auto logPath = dir / L"diagnostics.jsonl";
        const auto oldPath = dir / L"diagnostics.previous.jsonl";
        const auto logSize = std::filesystem::file_size(logPath, ec);
        if (!ec && logSize > 1024 * 1024) {
            std::filesystem::remove(oldPath, ec);
            ec.clear();
            std::filesystem::rename(logPath, oldPath, ec);
            if (ec) ec.clear();
        }
        ec.clear();
        std::ofstream out(logPath, std::ios::binary | std::ios::app);
        out << entry.dump() << "\n";
    } catch (...) {
    }
}

bool requestFileMatches(const std::filesystem::path& path, const std::string& id) {
    try {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > kRequestLimit) return false;
        std::ifstream input(path, std::ios::binary);
        if (!input) return false;
        const json request = json::parse(input);
        return request.is_object() && request.contains("id") && request["id"].is_string() &&
            request["id"].get<std::string>() == id;
    } catch (...) {
        return false;
    }
}

bool writeBridgeResponse(const json& response, const std::string& expectedId,
                         const std::string& command) {
    try {
        const auto dir = exchangeDirectory();
        const auto requestPath = dir / L"request.json";
        if (expectedId.empty() || !requestFileMatches(requestPath, expectedId)) {
            writeDiagnostic("response_request_mismatch", expectedId, command);
            return true;
        }
        const auto tmp = dir / L"response.tmp";
        const auto dst = dir / L"response.json";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << response.dump();
            out.close();
            if (!out) return false;
        }
        if (!MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return false;
        if (requestFileMatches(requestPath, expectedId)) {
            std::error_code ec;
            std::filesystem::remove(requestPath, ec);
        }
        std::error_code ec;
        const auto markerName = std::wstring(L"cancel.") + std::filesystem::path(expectedId).wstring();
        std::filesystem::remove(dir / markerName, ec);
        return true;
    } catch (const std::exception&) {
        writeDiagnostic("response_write_failed", expectedId, command);
        return false;
    } catch (...) {
        writeDiagnostic("response_write_failed", expectedId, command);
        return false;
    }
}

void queueResponse(json response, const std::string& id, const std::string& command) {
    response["id"] = id;
    response["command"] = command;
    response["protocol_version"] = kProtocolVersion;
    response["plugin_version"] = DEVBRIDGE_VERSION;
    requestState.active = true;
    requestState.responsePending = true;
    requestState.id = id;
    requestState.command = command;
    requestState.response = std::move(response);
}

void queueError(const std::string& id, const std::string& command,
                const std::string& code, const std::string& message) {
    json response = {{"ok", false}, {"error_code", code}, {"error", safeErrorText(message)}};
    queueResponse(std::move(response), id, command);
    writeDiagnostic(code, id, command);
}

bool requestCancelled(const std::string& id) {
    if (id.empty()) return false;
    const auto markerName = std::wstring(L"cancel.") + std::filesystem::path(id).wstring();
    const auto marker = exchangeDirectory() / markerName;
    std::error_code ec;
    if (!std::filesystem::exists(marker, ec) || ec) return false;
    std::filesystem::remove(marker, ec);
    return true;
}

void checkRequestControl(const std::string& id, std::uint64_t deadlineMs) {
    if (requestCancelled(id)) fail("cancelled", "request cancelled by client");
    if (deadlineMs != 0 && unixTimeMs() >= deadlineMs)
        fail("timeout", "request deadline expired before completion");
}

std::uint64_t requestDeadline(const json& request, const std::filesystem::path& requestPath) {
    const std::uint64_t now = unixTimeMs();
    if (request.contains("expires_at_unix_ms")) {
        const json& value = request["expires_at_unix_ms"];
        if (!value.is_number_integer() || value.get<std::int64_t>() <= 0)
            fail("invalid_request", "expires_at_unix_ms must be a positive integer");
        const std::uint64_t deadline = value.get<std::uint64_t>();
        if (deadline > now + kMaximumRequestLifetimeMs)
            fail("invalid_request", "request deadline exceeds the 180 second limit");
        return deadline;
    }

    std::error_code ec;
    const auto modified = std::filesystem::last_write_time(requestPath, ec);
    if (ec) fail("request_file_error", "cannot read request timestamp");
    const auto age = std::filesystem::file_time_type::clock::now() - modified;
    const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(age).count();
    if (ageMs < 0 || static_cast<std::uint64_t>(ageMs) >= kLegacyRequestLifetimeMs)
        return now;
    return now + kLegacyRequestLifetimeMs - static_cast<std::uint64_t>(ageMs);
}
void startRayBatch(const json& request,const std::string& id) {
    const auto& list=request.at("probes");
    if(!list.is_array()||list.empty()||list.size()>kBatchLimit)throw std::runtime_error("probes must contain 1..256 items");
    const int defaults=request.value("default_flags",1058);
    if(defaults<0||defaults>32767)throw std::runtime_error("invalid default_flags");
    PendingScan job;job.active=true;job.id=id;job.deadlineMs=requestState.deadlineMs;
    job.world=(ogame!=nullptr)?ogame->GetGameWorld():nullptr;
    job.probes.reserve(list.size());job.rows.resize(list.size());
    for(size_t i=0;i<list.size();++i){const auto& p=list.at(i);const int flags=p.value("flags",defaults);
        if(flags<0||flags>32767)throw std::runtime_error("invalid probe flags");
        job.probes.push_back(RayProbe{readVec(p.at("start")),readVec(p.at("direction")),flags});
        job.rows[i]=json{{"index",i}};
    }
    pendingScan=std::move(job);
}
void startSurfaceGrid(const json& request,const std::string& id) {
    const zVEC3 center=readVec(request.at("center"));
    const float rx=request.at("radius_x").get<float>(),rz=request.at("radius_z").get<float>();
    const float spacing=request.at("spacing").get<float>(),down=request.at("down_range").get<float>();
    if(!std::isfinite(rx)||!std::isfinite(rz)||!std::isfinite(spacing)||!std::isfinite(down)||
       rx<0||rz<0||rx>5000||rz>5000||spacing<10||spacing>5000||down<10||down>100000)
        throw std::runtime_error("invalid grid range; radii 0..5000, spacing 10..5000, down_range 10..100000");
    const int nx=static_cast<int>(std::floor((2*rx)/spacing))+1,nz=static_cast<int>(std::floor((2*rz)/spacing))+1;
    if(nx<=0||nz<=0||nx*nz>kGridLimit)throw std::runtime_error("grid exceeds 121 samples");
    PendingScan job;job.active=true;job.grid=true;job.includeBBox=request.value("include_bbox",false);job.id=id;
    job.deadlineMs=requestState.deadlineMs;job.world=(ogame!=nullptr)?ogame->GetGameWorld():nullptr;
    job.countX=nx;job.countZ=nz;job.spacing=spacing;job.radiusX=rx;job.radiusZ=rz;job.probes.reserve(nx*nz);job.rows.resize(nx*nz);
    const float x0=center.n[0]-rx,z0=center.n[2]-rz;const zVEC3 direction(0,-down,0);
    for(int rzidx=0;rzidx<nz;++rzidx)for(int cx=0;cx<nx;++cx){
        const int i=rzidx*nx+cx;const float x=x0+cx*spacing,z=z0+rzidx*spacing;const zVEC3 start(x,center.n[1],z);
        job.probes.push_back(RayProbe{start,direction,0});
        job.rows[i]=json{{"index",i},{"x",x},{"z",z},{"ray_start_y",center.n[1]}};
    }
    pendingScan=std::move(job);
}
void processPendingScan() {
    if (!pendingScan.active) return;
    const std::string id = pendingScan.id;
    const std::string command = pendingScan.grid ? "surface_grid" : "raycast_batch";
    auto failScan = [&](const char* code, const char* message) {
        pendingScan = PendingScan{};
        queueError(id, command, code, message);
    };
    try {
        checkRequestControl(id, pendingScan.deadlineMs);
        zCWorld* world = (ogame != nullptr) ? ogame->GetGameWorld() : nullptr;
        if (!world) {
            failScan("world_not_ready", "no world is currently loaded");
            return;
        }
        if (world != pendingScan.world) {
            failScan("world_changed", "loaded world changed while the scan was running");
            return;
        }
        if (pendingScan.finished) {
            json response = {{"ok", true}, {"count", pendingScan.rows.size()},
                             {"results", pendingScan.rows}};
            if (pendingScan.grid) {
                response["grid"] = json{{"count_x", pendingScan.countX}, {"count_z", pendingScan.countZ},
                    {"spacing", pendingScan.spacing}, {"radius_x", pendingScan.radiusX},
                    {"radius_z", pendingScan.radiusZ}, {"horizontal_axes", "world X/Z"},
                    {"vertical_axis", "world Y"}};
                response["comparison"] = json{{"visual", "static world and VOb model polygons"},
                    {"static_world", "static world polygons only"},
                    {"vob_cd_dynamic", "VOb model polygons filtered to cdDyn"},
                    {"character_support", "player collision object's floor/water/ceiling query"},
                    {"bbox", pendingScan.includeBBox ? "VOb bounding box" : "not requested"}};
            }
            const size_t count = pendingScan.rows.size();
            pendingScan = PendingScan{};
            response["count"] = count;
            queueResponse(std::move(response), id, command);
            return;
        }

        const size_t cap = pendingScan.grid ? kGridPerFrame : kBatchPerFrame;
        size_t done = 0;
        while (pendingScan.next < pendingScan.probes.size() && done < cap) {
            checkRequestControl(id, pendingScan.deadlineMs);
            const size_t i = pendingScan.next++;
            const auto& probe = pendingScan.probes[i];
            if (!pendingScan.grid) {
                json row = rayResult(world, probe.start, probe.direction, probe.flags);
                row["index"] = i;
                pendingScan.rows[i] = std::move(row);
            } else {
                auto& row = pendingScan.rows[i];
                row["visual"] = rayResult(world, probe.start, probe.direction,
                    zTRACERAY_STAT_POLY | zTRACERAY_POLY_2SIDED | zTRACERAY_POLY_NORMAL);
                row["static_world"] = rayResult(world, probe.start, probe.direction,
                    zTRACERAY_VOB_IGNORE | zTRACERAY_STAT_POLY | zTRACERAY_POLY_2SIDED | zTRACERAY_POLY_NORMAL);
                row["vob_cd_dynamic"] = rayResult(world, probe.start, probe.direction,
                    zTRACERAY_STAT_IGNORE | zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_POLY_2SIDED | zTRACERAY_POLY_NORMAL);
                if (pendingScan.includeBBox)
                    row["vob_bbox"] = rayResult(world, probe.start, probe.direction,
                        zTRACERAY_STAT_IGNORE | zTRACERAY_VOB_BBOX | zTRACERAY_POLY_2SIDED);
                row["character_support"] = characterSupport(probe.start);
            }
            ++done;
        }
        if (pendingScan.next >= pendingScan.probes.size()) pendingScan.finished = true;
    } catch (const BridgeError& error) {
        failScan(error.code.c_str(), error.what());
    } catch (const std::exception& error) {
        failScan("scan_failed", safeErrorText(error.what()).c_str());
    } catch (...) {
        failScan("internal_error", "unexpected error during scan");
    }
}
zCWorld* currentWorld() { return ogame != nullptr ? ogame->GetGameWorld() : nullptr; }

struct CameraRestoreResult {
    bool worldMatched = false;
    bool transformRestored = false;
    bool aiRestored = false;
};

CameraRestoreResult releaseCameraOverride(bool restore) {
    CameraRestoreResult result;
    if (cameraControl.savedValid && restore && ogame != nullptr) {
        zCWorld* world = currentWorld();
        result.worldMatched = world != nullptr && world == cameraControl.savedWorld;
        if (result.worldMatched) {
            zCCamera* camera = ogame->GetCamera();
            zCVob* cameraVob = ogame->GetCameraVob();
            zCAICamera* ai = ogame->GetCameraAI();
            if (camera == cameraControl.savedCamera && cameraVob == cameraControl.activeCameraVob) {
                cameraVob->SetTrafoObjToWorld(cameraControl.savedTransform);
                camera->SetTransform(zCAM_TRAFO_WORLD, cameraControl.savedCameraWorldTransform);
                result.transformRestored = true;
            }
            if (cameraControl.savedAIValid && ai == cameraControl.savedAI) {
                ai->translate = cameraControl.savedAITranslate;
                ai->rotate = cameraControl.savedAIRotate;
                result.aiRestored = true;
            }
        }
    }
    cameraControl.overrideActive = false;
    cameraControl.savedValid = false;
    cameraControl.savedAIValid = false;
    cameraControl.savedWorld = nullptr;
    cameraControl.savedCamera = nullptr;
    cameraControl.savedAI = nullptr;
    cameraControl.activeCamera = nullptr;
    cameraControl.activeCameraVob = nullptr;
    return result;
}

void observeWorldChange() {
    zCWorld* world = currentWorld();
    if (!observedWorldInitialized) {
        observedWorld = world;
        observedWorldInitialized = true;
        return;
    }
    if (world != observedWorld) {
        releaseCameraOverride(false);
        cameraControl.lastScenarioDispatched = 0;
        cameraControl.lastScenarioName.clear();
        observedWorld = world;
    }
}

json bridgeCapabilities() {
    loadBridgeSettings();
    const BridgeSettings& settings = bridgeSettings();
    json readOnly = {
        {"ping", true}, {"vob", true}, {"raycast", true}, {"raycast_batch", true},
        {"surface_grid", true}, {"camera_get", true},
        {"scenario_status", settings.configValid && !settings.statusSymbols.empty()}
    };
    json mutating = {
        {"camera_set", settings.configValid && settings.allowCameraControl},
        {"scenario_start", settings.configValid && settings.allowScenarioStart &&
            !settings.scenarioRequestSymbol.empty() && !settings.scenarios.empty()},
        {"camera_restore", true}
    };
    json result = {{"read_only", readOnly}, {"mutating", mutating},
                   {"config_valid", settings.configValid},
                   {"camera_control_enabled", settings.configValid && settings.allowCameraControl},
                   {"scenario_dispatch_enabled", settings.configValid && settings.allowScenarioStart &&
                       !settings.scenarioRequestSymbol.empty() && !settings.scenarios.empty()}};
    if (!settings.configValid) result["config_error"] = settings.configError;
    return result;
}

bool validRequestId(const std::string& id) {
    if (id.empty() || id.size() > 80 || id == "." || id == "..") return false;
    for (unsigned char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return false;
    }
    return true;
}

void writeHookHeartbeat() {
    static std::atomic_flag written = ATOMIC_FLAG_INIT;
    if (written.test_and_set()) return;
    try {
        const auto dir = exchangeDirectory();
        if (dir.empty()) return;
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            writeDiagnostic("exchange_directory_unavailable");
            return;
        }
        json status = {{"event", "hook_ready"}, {"pid", GetCurrentProcessId()},
                       {"plugin", "ZenGinDevBridge"}, {"plugin_version", DEVBRIDGE_VERSION},
                       {"protocol_version", kProtocolVersion}, {"world_ready", currentWorld() != nullptr}};
        const BridgeSettings& settings = bridgeSettings();
        status["config_valid"] = settings.configValid;
        if (!settings.configValid) status["config_error"] = settings.configError;
        const auto tmp = dir / L"plugin-heartbeat.tmp";
        const auto target = dir / L"plugin-heartbeat.json";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << status.dump();
            out.close();
            if (!out) {
                writeDiagnostic("heartbeat_write_failed");
                return;
            }
        }
        if (!MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            writeDiagnostic("heartbeat_publish_failed");
    } catch (...) {
        writeDiagnostic("heartbeat_error");
    }
}

void processRequest() {
    if (requestState.responsePending) {
        if (writeBridgeResponse(requestState.response, requestState.id, requestState.command))
            requestState = RequestState{};
        return;
    }

    const auto dir = exchangeDirectory();
    const auto path = dir / L"request.json";
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec == std::errc::no_such_file_or_directory) return;
    if (ec) {
        writeDiagnostic("request_file_unavailable");
        return;
    }
    if (size > kRequestLimit) {
        writeDiagnostic("request_too_large");
        std::filesystem::remove(path, ec);
        return;
    }

    json request;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            writeDiagnostic("request_open_failed");
            return;
        }
        request = json::parse(input);
        if (!request.is_object() || !request.contains("id") || !request["id"].is_string() ||
            !request.contains("command") || !request["command"].is_string()) {
            writeDiagnostic("malformed_request");
            std::filesystem::remove(path, ec);
            return;
        }
    } catch (...) {
        writeDiagnostic("malformed_request");
        std::filesystem::remove(path, ec);
        return;
    }

    const std::string id = request.at("id").get<std::string>();
    const std::string command = request.at("command").get<std::string>();
    if (!validRequestId(id) || command.empty() || command.size() > 64 ||
        command.find('\0') != std::string::npos) {
        writeDiagnostic("invalid_request_identity");
        std::filesystem::remove(path, ec);
        return;
    }

    requestState = RequestState{};
    requestState.active = true;
    requestState.id = id;
    requestState.command = command;
    try {
        int protocolVersion = kProtocolVersion;
        if (request.contains("protocol_version")) {
            if (!request["protocol_version"].is_number_integer())
                fail("invalid_request", "protocol_version must be an integer");
            protocolVersion = request["protocol_version"].get<int>();
        }
        if (protocolVersion != kProtocolVersion) {
            json response = {{"ok", false}, {"error_code", "protocol_version_unsupported"},
                {"error", "unsupported protocol version"}, {"supported_protocol_versions", json::array({kProtocolVersion})}};
            queueResponse(std::move(response), id, command);
            writeDiagnostic("protocol_version_unsupported", id, command);
        } else {
            requestState.deadlineMs = requestDeadline(request, path);
            if (requestState.deadlineMs <= unixTimeMs())
                fail("timeout", "request deadline expired before it was processed");
            if (command != "camera_restore") checkRequestControl(id, requestState.deadlineMs);

            zCWorld* world = currentWorld();
            json result = {{"ok", true}};
            if (command == "ping") {
                result["plugin"] = "ZenGinDevBridge";
                result["plugin_version"] = DEVBRIDGE_VERSION;
                result["protocol_version"] = kProtocolVersion;
                result["world_ready"] = world != nullptr;
                result["stage"] = 2;
                result["capabilities"] = bridgeCapabilities();
            } else if (command == "vob") {
                if (!world) fail("world_not_ready", "no world is currently loaded");
                const auto name = request.at("name").get<std::string>();
                if (name.empty() || name.size() > 128 || name.find('\0') != std::string::npos)
                    fail("invalid_request", "invalid VOb name");
                const auto cp = transcode(name, CP_UTF8, 1250);
                zCVob* vob = world->SearchVobByName(zSTRING(cp.c_str()));
                result["found"] = vob != nullptr;
                if (vob) {
                    result["name"] = transcode(vob->GetObjectName().ToChar(), 1250, CP_UTF8);
                    result["class"] = className(vob);
                    result["matrix"] = matrixJson(vob->trafoObjToWorld);
                    result["bbox3d_min"] = vec(vob->bbox3D.mins);
                    result["bbox3d_max"] = vec(vob->bbox3D.maxs);
                    result["cd_static"] = static_cast<bool>(vob->collDetectionStatic);
                    result["cd_dynamic"] = static_cast<bool>(vob->collDetectionDynamic);
                    result["collision_object_present"] = vob->GetCollisionObject() != nullptr;
                }
            } else if (command == "raycast") {
                if (!world) fail("world_not_ready", "no world is currently loaded");
                const auto start = readVec(request.at("start"));
                const auto ray = readVec(request.at("direction"));
                const int flags = request.at("flags").get<int>();
                if (flags < 0 || flags > 32767) fail("invalid_request", "invalid ray flags");
                result = rayResult(world, start, ray, flags);
            } else if (command == "raycast_batch" || command == "surface_grid") {
                if (!world) fail("world_not_ready", "no world is currently loaded");
                if (command == "raycast_batch") startRayBatch(request, id);
                else startSurfaceGrid(request, id);
                return;
            } else if (command == "scenario_start") {
                const BridgeSettings& settings = bridgeSettings();
                if (!settings.configValid || !settings.allowScenarioStart ||
                    settings.scenarioRequestSymbol.empty() || settings.scenarios.empty())
                    fail("capability_disabled", "scenario dispatch is disabled or not configured");
                if (!world || !ogame) fail("world_not_ready", "no world is currently loaded");
                if (!request.contains("scenario") || !request["scenario"].is_string())
                    fail("invalid_request", "scenario_start requires a configured scenario name");
                const std::string scenarioName = request.at("scenario").get<std::string>();
                if (!settings.scenarios.contains(scenarioName))
                    fail("unsupported_scenario", "scenario name is not present in local configuration");
                const int scenarioId = settings.scenarios.at(scenarioName).get<int>();
                if (cameraControl.lastScenarioDispatched == scenarioId &&
                    cameraControl.lastScenarioName == scenarioName) {
                    result["already_dispatched"] = true;
                    result["scenario"] = scenarioName;
                    result["scenario_id"] = scenarioId;
                } else {
                    checkRequestControl(id, requestState.deadlineMs);
                    if (!parser) fail("parser_unavailable", "Daedalus parser is unavailable");
                    zSTRING requestName(settings.scenarioRequestSymbol.c_str());
                    const int requestIndex = parser->GetIndex(requestName);
                    if (requestIndex < 0) fail("configured_symbol_missing", "scenario queue symbol was not found");
                    zCPar_Symbol* requestSymbol = parser->GetSymbol(requestIndex);
                    if (!requestSymbol) fail("configured_symbol_unavailable", "scenario queue symbol is unavailable");
                    int pendingRequest = 0;
                    requestSymbol->GetValue(pendingRequest, 0);
                    if (pendingRequest != 0)
                        fail("scenario_queue_busy", "another scenario request is still pending");
                    checkRequestControl(id, requestState.deadlineMs);
                    requestSymbol->SetValue(scenarioId, 0);
                    cameraControl.lastScenarioDispatched = scenarioId;
                    cameraControl.lastScenarioName = scenarioName;
                    result["scenario"] = scenarioName;
                    result["scenario_id"] = scenarioId;
                    result["request_symbol"] = settings.scenarioRequestSymbol;
                    result["request_symbol_index"] = requestIndex;
                    result["queued_via"] = "configured integer symbol";
                }
            } else if (command == "scenario_status") {
                const BridgeSettings& settings = bridgeSettings();
                if (!settings.configValid || settings.statusSymbols.empty())
                    fail("capability_disabled", "scenario status symbols are not configured");
                if (!world || !parser) fail("world_not_ready", "world or Daedalus parser is unavailable");
                json values = json::object();
                for (auto it = settings.statusSymbols.begin(); it != settings.statusSymbols.end(); ++it) {
                    const std::string symbolNameText = it.value().get<std::string>();
                    zSTRING symbolName(symbolNameText.c_str());
                    const int index = parser->GetIndex(symbolName);
                    if (index < 0) fail("configured_symbol_missing", "a configured status symbol was not found");
                    zCPar_Symbol* symbol = parser->GetSymbol(index);
                    if (!symbol) fail("configured_symbol_unavailable", "a configured status symbol is unavailable");
                    int value = 0;
                    symbol->GetValue(value, 0);
                    values[it.key()] = value;
                    result[it.key()] = value;
                }
                result["scenario_status"] = std::move(values);
                result["last_dispatched_scenario_id"] = cameraControl.lastScenarioDispatched;
                if (!cameraControl.lastScenarioName.empty())
                    result["last_dispatched_scenario"] = cameraControl.lastScenarioName;
            } else if (command == "camera_get") {
                if (!world || !ogame) fail("world_not_ready", "no world is currently loaded");
                zCCamera* camera = ogame->GetCamera();
                if (!camera) fail("camera_unavailable", "camera is unavailable");
                zCVob* camVob = ogame->GetCameraVob();
                zCAICamera* ai = ogame->GetCameraAI();
                if (!camVob) fail("camera_unavailable", "camera VOb is unavailable");
                const zMAT4 worldMatrix = camera->GetTransform(zCAM_TRAFO_VIEW_INV);
                const zMAT4 viewMatrix = camera->GetTransform(zCAM_TRAFO_VIEW);
                result["position"] = vec(worldMatrix.GetTranslation());
                result["right"] = vec(worldMatrix.GetRightVector());
                result["up"] = vec(worldMatrix.GetUpVector());
                result["at"] = vec(worldMatrix.GetAtVector());
                result["world_matrix"] = matrixJson(worldMatrix);
                result["view_matrix"] = matrixJson(viewMatrix);
                result["camera_matrix_source"] = "zCCamera::GetTransform(zCAM_TRAFO_VIEW_INV)";
                result["camera_vob_world_matrix"] = matrixJson(camVob->trafoObjToWorld);
                result["override_active"] = cameraControl.overrideActive;
                result["camera_vob_name"] = transcode(camVob->GetObjectName().ToChar(), 1250, CP_UTF8);
                result["camera_vob_class"] = className(camVob);
                result["camera_ai_present"] = ai != nullptr;
                if (ai) {
                    const zSTRING mode = ai->GetMode();
                    result["camera_ai_mode"] = transcode(mode.ToChar(), 1250, CP_UTF8);
                    result["camera_ai_translate"] = static_cast<bool>(ai->translate);
                    result["camera_ai_rotate"] = static_cast<bool>(ai->rotate);
                }
            } else if (command == "camera_set") {
                const BridgeSettings& settings = bridgeSettings();
                if (!settings.configValid || !settings.allowCameraControl)
                    fail("capability_disabled", "camera control is disabled in local configuration");
                if (!world || !ogame) fail("world_not_ready", "no world is currently loaded");
                zCCamera* camera = ogame->GetCamera();
                zCVob* camVob = ogame->GetCameraVob();
                zCAICamera* ai = ogame->GetCameraAI();
                if (!camera || !camVob) fail("camera_unavailable", "camera or camera VOb is unavailable");
                const zVEC3 position = readVec(request.at("position"));
                zVEC3 target(0, 0, 0);
                if (request.contains("look_at")) {
                    target = readVec(request.at("look_at"));
                } else if (request.contains("target_vob")) {
                    const auto name = request.at("target_vob").get<std::string>();
                    if (name.empty() || name.size() > 128 || name.find('\0') != std::string::npos)
                        fail("invalid_request", "invalid target VOb name");
                    const auto cp = transcode(name, CP_UTF8, 1250);
                    zCVob* vob = world->SearchVobByName(zSTRING(cp.c_str()));
                    if (!vob) fail("target_vob_not_found", "target VOb was not found");
                    target = zVEC3((vob->bbox3D.mins.n[0] + vob->bbox3D.maxs.n[0]) * 0.5f,
                                   (vob->bbox3D.mins.n[1] + vob->bbox3D.maxs.n[1]) * 0.5f,
                                   (vob->bbox3D.mins.n[2] + vob->bbox3D.maxs.n[2]) * 0.5f);
                    result["target_vob"] = transcode(vob->GetObjectName().ToChar(), 1250, CP_UTF8);
                    result["target_vob_class"] = className(vob);
                } else {
                    fail("invalid_request", "camera_set requires look_at or target_vob");
                }
                const zMAT4 pose = cameraLookAt(position, target);
                if (cameraControl.savedValid &&
                    (cameraControl.savedWorld != world || cameraControl.savedCamera != camera ||
                     cameraControl.activeCameraVob != camVob))
                    releaseCameraOverride(cameraControl.savedWorld == world);
                if (!cameraControl.savedValid) {
                    cameraControl.savedWorld = world;
                    cameraControl.savedCamera = camera;
                    cameraControl.savedTransform = camVob->trafoObjToWorld;
                    cameraControl.savedCameraWorldTransform = camera->GetTransform(zCAM_TRAFO_WORLD);
                    cameraControl.savedValid = true;
                    cameraControl.savedAI = ai;
                    cameraControl.savedAIValid = ai != nullptr;
                    if (ai) {
                        cameraControl.savedAITranslate = ai->translate;
                        cameraControl.savedAIRotate = ai->rotate;
                    }
                } else if (ai != cameraControl.savedAI) {
                    cameraControl.savedAI = ai;
                    cameraControl.savedAIValid = ai != nullptr;
                    if (ai) {
                        cameraControl.savedAITranslate = ai->translate;
                        cameraControl.savedAIRotate = ai->rotate;
                    }
                }
                checkRequestControl(id, requestState.deadlineMs);
                if (ai) {
                    ai->translate = 0;
                    ai->rotate = 0;
                }
                cameraControl.activeCamera = camera;
                cameraControl.activeCameraVob = camVob;
                cameraControl.overrideTransform = pose;
                camVob->SetTrafoObjToWorld(pose);
                camera->SetTransform(zCAM_TRAFO_WORLD, pose);
                cameraControl.overrideActive = true;
                result["override_active"] = true;
                result["position"] = vec(position);
                result["look_at"] = vec(target);
                result["world_matrix"] = matrixJson(pose);
            } else if (command == "camera_restore") {
                const CameraRestoreResult restored = releaseCameraOverride(true);
                result["override_active"] = false;
                result["restored"] = restored.transformRestored;
                result["camera_ai_restored"] = restored.aiRestored;
                result["world_matched"] = restored.worldMatched;
                if (!restored.transformRestored)
                    result["message"] = "camera restore skipped because the saved camera or world is unavailable or changed";
                else
                    result["message"] = "saved camera transform restored";
            } else {
                fail("unknown_command", "unknown command");
            }
            queueResponse(std::move(result), id, command);
        }
    } catch (const BridgeError& error) {
        queueError(id, command, error.code, error.what());
    } catch (const std::exception& error) {
        queueError(id, command, "invalid_request", safeErrorText(error.what()));
    } catch (...) {
        queueError(id, command, "internal_error", "unexpected plugin error");
    }

    if (requestState.responsePending && writeBridgeResponse(requestState.response, id, command))
        requestState = RequestState{};
}

void __fastcall BridgeRenderBlit(oCGame* self, void* vtable);
auto RenderBlitHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::RenderBlit), &BridgeRenderBlit, Union::HookType::Hook_Detours);

void __fastcall BridgeCameraActivate(zCCamera* self, void* edx);
auto CameraActivateHook = Union::CreateHook(
    SIGNATURE_OF(&zCCamera::Activate), &BridgeCameraActivate, Union::HookType::Hook_Detours);

void __fastcall BridgeCameraActivate(zCCamera* self, void* edx) {
    if (cameraControl.overrideActive) {
        const bool sameWorld = currentWorld() != nullptr && currentWorld() == cameraControl.savedWorld;
        const bool sameCamera = sameWorld && ogame != nullptr && self != nullptr &&
            self == cameraControl.activeCamera && ogame->GetCamera() == cameraControl.activeCamera;
        const bool sameCameraVob = sameCamera && ogame->GetCameraVob() == cameraControl.activeCameraVob;
        if (!sameCameraVob) {
            releaseCameraOverride(false);
        } else {
            cameraControl.activeCameraVob->SetTrafoObjToWorld(cameraControl.overrideTransform);
            self->SetTransform(zCAM_TRAFO_WORLD, cameraControl.overrideTransform);
        }
    }
    CameraActivateHook(self,edx);
}

void __fastcall BridgeRenderBlit(oCGame* self, void* vtable) {
    writeHookHeartbeat();
    try {
        observeWorldChange();
        if (pendingScan.active) {
            processPendingScan();
        } else {
            static DWORD lastPoll = 0;
            const DWORD now = GetTickCount();
            if (static_cast<DWORD>(now - lastPoll) >= 200) {
                lastPoll = now;
                processRequest();
            }
        }
    } catch (...) {
        writeDiagnostic("render_hook_processing_error");
    }
    RenderBlitHook(self, vtable);
}
}
}
