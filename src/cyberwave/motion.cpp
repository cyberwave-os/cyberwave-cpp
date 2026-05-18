#include "cyberwave/motion.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin.h"

#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/MotionPlanSchema.h"
#include "CppRestOpenAPIClient/model/TwinActionRequestSchema.h"
#include "CppRestOpenAPIClient/model/TwinMotionResponseSchema.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>
#include <pplx/pplxtasks.h>

namespace cyberwave
{

namespace
{

org::openapitools::client::api::DefaultApi* api(const Client& client) { return ClientAccess::default_api(&client); }

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

org::openapitools::client::model::TwinActionRequestSchema::Action_typeEnum
action_type_from_string(const std::string& action_type)
{
    using ActionType = org::openapitools::client::model::TwinActionRequestSchema::Action_typeEnum;
    if (action_type == "pose")
        return ActionType::POSE;
    if (action_type == "movement")
        return ActionType::MOVEMENT;
    if (action_type == "animation")
        return ActionType::ANIMATION;
    if (action_type == "plan")
        return ActionType::PLAN;
    throw CyberwaveError("Unsupported twin motion action type: " + action_type);
}

// Fill common rich params into a TwinActionRequestSchema.
void fill_action_rich(org::openapitools::client::model::TwinActionRequestSchema& req,
                      const std::string& environment_uuid, bool preview, bool sync, const std::string& source_type,
                      int transition_ms, int hold_ms, const std::string& scope)
{
    if (!environment_uuid.empty())
        req.setEnvironmentUuid(from_std(environment_uuid));
    if (preview)
        req.setPreview(true);
    if (!sync)
        req.setExecution(org::openapitools::client::model::TwinActionRequestSchema::ExecutionEnum::ASYNC);
    if (!source_type.empty())
        req.setSourceType(from_std(source_type));
    if (!scope.empty())
        req.setScope(from_std(scope));
    if (transition_ms >= 0)
        req.setTransitionMs(static_cast<int32_t>(transition_ms));
    if (hold_ms >= 0)
        req.setHoldMs(static_cast<int32_t>(hold_ms));
}

bool matches_scope(const std::string& encoded, const std::string& scope)
{
    if (scope.empty() || scope == "auto")
        return true;
    const auto parsed = web::json::value::parse(from_std(encoded));
    if (!parsed.is_object() || !parsed.has_field(from_std("scope")))
        return false;
    const auto& scope_value = parsed.at(from_std("scope"));
    return scope_value.is_string() && utility::conversions::to_utf8string(scope_value.as_string()) == scope;
}

} // namespace

// --- ScopedMotionHandle ---

ScopedMotionHandle::ScopedMotionHandle(Twin twin, std::string env_uuid, std::string scope)
    : twin_(std::move(twin)), env_uuid_(std::move(env_uuid)), scope_(std::move(scope))
{
}

void ScopedMotionHandle::pose(const std::string& name, bool preview, bool sync, const std::string& source_type,
                              int transition_ms, int hold_ms, const std::map<std::string, double>& joints) const
{
    TwinMotionHandle(twin_).pose(name, env_uuid_, preview, sync, source_type, transition_ms, hold_ms, joints, scope_);
}

void ScopedMotionHandle::animation(const std::string& name, bool preview, bool sync, const std::string& source_type,
                                   int transition_ms, int hold_ms) const
{
    TwinMotionHandle(twin_).animation(name, env_uuid_, preview, sync, source_type, transition_ms, hold_ms, scope_);
}

void ScopedMotionHandle::plan(const std::string& plan_json, bool preview, bool sync, const std::string& source_type,
                              int tick_ms) const
{
    TwinMotionHandle(twin_).plan(plan_json, env_uuid_, preview, sync, source_type, tick_ms, scope_);
}

std::vector<std::string> ScopedMotionHandle::list_keyframes() const
{
    return TwinMotionHandle(twin_).list_keyframes(env_uuid_, scope_);
}

std::vector<std::string> ScopedMotionHandle::list_animations() const
{
    return TwinMotionHandle(twin_).list_animations(env_uuid_, scope_);
}

// --- TwinMotionHandle ---

TwinMotionHandle::TwinMotionHandle(Twin twin) : twin_(std::move(twin)) {}

const std::string& TwinMotionHandle::twin_uuid() const { return twin_.uuid(); }

std::vector<std::string> TwinMotionHandle::list_keyframes(const std::string& environment_uuid,
                                                          const std::string& scope) const
{
    auto* a = api(twin_.client());
    if (!a)
        return {};
    boost::optional<utility::string_t> env_opt;
    if (!environment_uuid.empty())
        env_opt = from_std(environment_uuid);
    auto resp = a->srcAppApiTwinsGetTwinMotions(from_std(twin_.uuid()), env_opt).get();
    std::vector<std::string> out;
    if (!resp || !resp->keyframesIsSet())
        return out;
    for (const auto& kf : resp->getKeyframes())
    {
        if (kf)
        {
            const auto encoded = utility::conversions::to_utf8string(kf->toJson().serialize());
            if (matches_scope(encoded, scope))
                out.push_back(encoded);
        }
    }
    return out;
}

std::vector<std::string> TwinMotionHandle::list_animations(const std::string& environment_uuid,
                                                           const std::string& scope) const
{
    auto* a = api(twin_.client());
    if (!a)
        return {};
    boost::optional<utility::string_t> env_opt;
    if (!environment_uuid.empty())
        env_opt = from_std(environment_uuid);
    auto resp = a->srcAppApiTwinsGetTwinMotions(from_std(twin_.uuid()), env_opt).get();
    std::vector<std::string> out;
    if (!resp || !resp->animationsIsSet())
        return out;
    for (const auto& anim : resp->getAnimations())
    {
        if (anim)
        {
            const auto encoded = utility::conversions::to_utf8string(anim->toJson().serialize());
            if (matches_scope(encoded, scope))
                out.push_back(encoded);
        }
    }
    return out;
}

void TwinMotionHandle::pose(const std::string& name, const std::string& environment_uuid, bool preview, bool sync,
                            const std::string& source_type, int transition_ms, int hold_ms,
                            const std::map<std::string, double>& joints, const std::string& scope) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto req = std::make_shared<org::openapitools::client::model::TwinActionRequestSchema>();
    req->setActionType(action_type_from_string("pose"));
    if (!name.empty())
        req->setName(from_std(name));
    fill_action_rich(*req, environment_uuid, preview, sync, source_type, transition_ms, hold_ms, scope);
    if (!joints.empty())
    {
        std::map<utility::string_t, double> js;
        for (const auto& kv : joints)
            js[from_std(kv.first)] = kv.second;
        req->setJoints(js);
    }
    a->srcAppApiTwinsExecuteTwinAction(from_std(twin_.uuid()), req).get();
}

void TwinMotionHandle::animation(const std::string& name, const std::string& environment_uuid, bool preview, bool sync,
                                 const std::string& source_type, int transition_ms, int hold_ms,
                                 const std::string& scope) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto req = std::make_shared<org::openapitools::client::model::TwinActionRequestSchema>();
    req->setActionType(action_type_from_string("animation"));
    req->setName(from_std(name));
    fill_action_rich(*req, environment_uuid, preview, sync, source_type, transition_ms, hold_ms, scope);
    a->srcAppApiTwinsExecuteTwinAction(from_std(twin_.uuid()), req).get();
}

void TwinMotionHandle::plan(const std::string& plan_json, const std::string& environment_uuid, bool preview, bool sync,
                            const std::string& source_type, int tick_ms, const std::string& scope) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto req = std::make_shared<org::openapitools::client::model::TwinActionRequestSchema>();
    req->setActionType(action_type_from_string("plan"));
    web::json::value j = web::json::value::parse(from_std(plan_json));
    auto plan_schema = std::make_shared<org::openapitools::client::model::MotionPlanSchema>();
    if (plan_schema->fromJson(j))
        req->setPlan(plan_schema);
    fill_action_rich(*req, environment_uuid, preview, sync, source_type, -1, -1, scope);
    if (tick_ms >= 0)
        req->setTickMs(static_cast<int32_t>(tick_ms));
    a->srcAppApiTwinsExecuteTwinAction(from_std(twin_.uuid()), req).get();
}

void TwinMotionHandle::plan_legacy(const std::string& action_type, const std::string& plan_json) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto req = std::make_shared<org::openapitools::client::model::TwinActionRequestSchema>();
    req->setActionType(action_type_from_string(action_type));
    web::json::value j = web::json::value::parse(from_std(plan_json));
    auto plan_schema = std::make_shared<org::openapitools::client::model::MotionPlanSchema>();
    if (plan_schema->fromJson(j))
        req->setPlan(plan_schema);
    a->srcAppApiTwinsExecuteTwinAction(from_std(twin_.uuid()), req).get();
}

ScopedMotionHandle TwinMotionHandle::in_environment(const std::string& environment_uuid) const
{
    return ScopedMotionHandle(twin_, environment_uuid, "environment");
}

ScopedMotionHandle TwinMotionHandle::environment() const { return ScopedMotionHandle(twin_, "", "environment"); }

ScopedMotionHandle TwinMotionHandle::twin() const { return ScopedMotionHandle(twin_, "", "twin"); }

ScopedMotionHandle TwinMotionHandle::asset() const { return ScopedMotionHandle(twin_, "", "asset"); }

} // namespace cyberwave
