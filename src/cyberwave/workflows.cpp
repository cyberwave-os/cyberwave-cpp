#include "cyberwave/workflows.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/rest_helpers.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/WorkflowRunSchema.h"
#include "CppRestOpenAPIClient/model/WorkflowSchema.h"
#include "CppRestOpenAPIClient/model/WorkflowTriggerSchema.h"

#include <boost/optional.hpp>
#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>
#include <nlohmann/json.hpp>
#include <pplx/pplxtasks.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <thread>

namespace cyberwave
{

namespace
{

std::string to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

static org::openapitools::client::api::DefaultApi* api(const Client& client)
{
    return ClientAccess::default_api(&client);
}

static std::shared_ptr<org::openapitools::client::model::WorkflowSchema> workflow_schema(const std::shared_ptr<void>& p)
{
    return std::static_pointer_cast<org::openapitools::client::model::WorkflowSchema>(p);
}

static std::shared_ptr<org::openapitools::client::model::WorkflowRunSchema> run_schema(const std::shared_ptr<void>& p)
{
    return std::static_pointer_cast<org::openapitools::client::model::WorkflowRunSchema>(p);
}

} // namespace

// -----------------------------------------------------------------------------
// Workflow
// -----------------------------------------------------------------------------

Workflow::Workflow(const Client& client, std::shared_ptr<void> schema_ptr)
    : client_(client), schema_(std::move(schema_ptr))
{
}

std::string Workflow::uuid() const
{
    auto s = workflow_schema(schema_);
    return s && s->uuidIsSet() ? to_std(s->getUuid()) : "";
}

std::string Workflow::slug() const
{
    auto s = workflow_schema(schema_);
    return s && s->slugIsSet() ? to_std(s->getSlug()) : "";
}

std::string Workflow::name() const
{
    auto s = workflow_schema(schema_);
    return s && s->nameIsSet() ? to_std(s->getName()) : "";
}

std::string Workflow::description() const
{
    auto s = workflow_schema(schema_);
    return s && s->descriptionIsSet() ? to_std(s->getDescription()) : "";
}

bool Workflow::is_active() const
{
    auto s = workflow_schema(schema_);
    return s && s->isActiveIsSet() && s->isIsActive();
}

std::string Workflow::status() const { return is_active() ? "active" : "inactive"; }

std::string Workflow::workspace_uuid() const
{
    auto s = workflow_schema(schema_);
    return s && s->workspaceUuidIsSet() ? to_std(s->getWorkspaceUuid()) : "";
}

std::string Workflow::visibility() const
{
    auto s = workflow_schema(schema_);
    return s && s->visibilityIsSet() ? to_std(s->getVisibility()) : "";
}

std::string Workflow::created_at() const
{
    auto s = workflow_schema(schema_);
    if (!s || !s->createdAtIsSet())
        return "";
    return to_std(s->getCreatedAt().to_string(utility::datetime::ISO_8601));
}

std::string Workflow::updated_at() const
{
    auto s = workflow_schema(schema_);
    if (!s || !s->updatedAtIsSet())
        return "";
    return to_std(s->getUpdatedAt().to_string(utility::datetime::ISO_8601));
}

std::string Workflow::metadata_json() const
{
    auto s = workflow_schema(schema_);
    if (!s || !s->metadataIsSet())
        return "{}";
    web::json::value obj = web::json::value::object();
    for (const auto& kv : s->getMetadata())
    {
        if (kv.second)
            obj[kv.first] = kv.second->toJson();
    }
    return to_std(obj.serialize());
}

WorkflowRun Workflow::trigger(const std::map<std::string, std::string>& inputs) const
{
    WorkflowManager mgr(client_.get());
    return mgr.trigger(uuid(), inputs);
}

WorkflowRun Workflow::trigger_with_json(const std::string& inputs_json) const
{
    WorkflowManager mgr(client_.get());
    return mgr.trigger_with_json(uuid(), inputs_json);
}

std::vector<WorkflowRun> Workflow::runs(const std::string& status_filter) const
{
    WorkflowRunManager mgr(client_.get());
    return mgr.list(uuid(), status_filter);
}

// -----------------------------------------------------------------------------
// WorkflowRun
// -----------------------------------------------------------------------------

WorkflowRun::WorkflowRun(const Client& client, std::shared_ptr<void> schema_ptr)
    : client_(client), schema_(std::move(schema_ptr)), client_lifetime_(ClientAccess::lifetime_token(&client))
{
}

std::string WorkflowRun::uuid() const
{
    auto s = run_schema(schema_);
    return s && s->uuidIsSet() ? to_std(s->getUuid()) : "";
}

std::string WorkflowRun::workflow_id() const
{
    auto s = run_schema(schema_);
    return s && s->workflowIdIsSet() ? to_std(s->getWorkflowId()) : "";
}

std::string WorkflowRun::status() const
{
    auto s = run_schema(schema_);
    return s && s->statusIsSet() ? to_std(s->getStatus()) : "";
}

std::string WorkflowRun::error() const
{
    auto s = run_schema(schema_);
    return s && s->errorIsSet() ? to_std(s->getError()) : "";
}

std::string WorkflowRun::started_at() const
{
    auto s = run_schema(schema_);
    if (!s || !s->startedAtIsSet())
        return "";
    return to_std(s->getStartedAt().to_string());
}

std::string WorkflowRun::finished_at() const
{
    auto s = run_schema(schema_);
    if (!s || !s->finishedAtIsSet())
        return "";
    return to_std(s->getFinishedAt().to_string());
}

std::string WorkflowRun::inputs_json() const
{
    auto s = run_schema(schema_);
    if (!s || !s->inputsIsSet())
        return "";
    web::json::value obj = web::json::value::object();
    for (const auto& kv : s->getInputs())
        if (kv.second)
            obj[kv.first] = kv.second->toJson();
    return to_std(obj.serialize());
}

std::string WorkflowRun::result_json() const
{
    auto s = run_schema(schema_);
    if (!s || !s->resultIsSet())
        return "";
    web::json::value obj = web::json::value::object();
    for (const auto& kv : s->getResult())
        if (kv.second)
            obj[kv.first] = kv.second->toJson();
    return to_std(obj.serialize());
}

bool WorkflowRun::is_terminal() const
{
    static const std::set<std::string> TERMINAL{"success", "error", "failed", "completed", "canceled", "cancelled"};
    return TERMINAL.count(status()) > 0;
}

double WorkflowRun::duration() const
{
    auto s = run_schema(schema_);
    if (!s || !s->startedAtIsSet() || !s->finishedAtIsSet())
        return -1.0;
    auto start = s->getStartedAt().to_interval();
    auto end = s->getFinishedAt().to_interval();
    return static_cast<double>(end - start) / 1e7; // 100-ns ticks → seconds
}

WorkflowRun& WorkflowRun::refresh()
{
    WorkflowRunManager mgr(client_.get());
    *this = mgr.get(uuid());
    return *this;
}

WorkflowRun& WorkflowRun::cancel()
{
    WorkflowRunManager mgr(client_.get());
    *this = mgr.cancel(uuid());
    return *this;
}

std::unique_ptr<MqttSubscriptionHandle>
WorkflowRun::on_status_change(const std::function<void(const std::string&, const WorkflowRun&)>& callback)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt)
    {
        throw CyberwaveMQTTError("WorkflowRun::on_status_change() requires an MQTT client set on Client");
    }
    if (!mqtt->is_connected())
    {
        mqtt->connect();
    }
    if (!schema_)
    {
        schema_ =
            std::static_pointer_cast<void>(std::make_shared<org::openapitools::client::model::WorkflowRunSchema>());
    }

    const std::string topic = mqtt->get_topic_prefix() + "cyberwave/workflow-run/" + uuid() + "/status";
    auto client_ref = client_;
    auto shared_schema = schema_;
    auto client_lifetime = client_lifetime_;
    return mqtt->subscribe_scoped(
        topic,
        [client_ref, shared_schema, client_lifetime, callback](const std::string& json_payload)
        {
            auto keepalive = client_lifetime.lock();
            if (!keepalive)
            {
                return;
            }

            std::string new_status;
            try
            {
                const auto payload = nlohmann::json::parse(json_payload);
                if (payload.is_object())
                {
                    const auto it = payload.find("status");
                    if (it != payload.end() && it->is_string())
                    {
                        new_status = it->get<std::string>();
                    }
                }
                else if (payload.is_string())
                {
                    new_status = payload.get<std::string>();
                }
            }
            catch (const nlohmann::json::parse_error&)
            {
            }

            if (!new_status.empty())
            {
                auto schema = run_schema(shared_schema);
                if (schema)
                {
                    schema->setStatus(from_std(new_status));
                }
            }

            callback(new_status, WorkflowRun(client_ref.get(), shared_schema));
        });
}

WorkflowRun& WorkflowRun::wait(double timeout_s, double poll_interval_s)
{
    if (is_terminal())
        return *this;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_s);
    while (!is_terminal())
    {
        if (std::chrono::steady_clock::now() >= deadline)
            throw CyberwaveTimeoutError("WorkflowRun " + uuid() + " did not complete within " +
                                        std::to_string(static_cast<int>(timeout_s)) + "s");
        std::this_thread::sleep_for(std::chrono::duration<double>(poll_interval_s));
        if (is_terminal())
            break;
        refresh();
    }
    return *this;
}

// -----------------------------------------------------------------------------
// WorkflowManager
// -----------------------------------------------------------------------------

WorkflowManager::WorkflowManager(const Client& client) : client_(client) {}

std::vector<Workflow> WorkflowManager::list() const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto vec = a->srcAppApiWorkflowsListWorkflows().get();
        std::vector<Workflow> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(Workflow(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Workflow WorkflowManager::get(const std::string& workflow_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiWorkflowsGetWorkflow(from_std(workflow_id)).get();
        if (!result)
            throw CyberwaveError("Get workflow returned no data");
        return Workflow(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Workflow WorkflowManager::get_by_slug(const std::string& workspace_id, const std::string& slug) const
{
    auto response = detail::request_raw(client_.get(), from_std("/api/v1/workflows"), web::http::methods::GET,
                                        {
                                            {from_std("workspace_uuid"), from_std(workspace_id)},
                                            {from_std("slug"), from_std(slug)},
                                        });

    const auto parsed = detail::parse_json_response(response);
    if (!parsed.is_array())
    {
        throw CyberwaveError("Workflow not found for slug: " + slug);
    }
    auto items = parsed.as_array();
    if (items.size() == 0)
        throw CyberwaveError("Workflow not found for slug: " + slug);

    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    schema->fromJson(items[0]);
    return Workflow(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
}

WorkflowRun WorkflowManager::trigger(const std::string& workflow_id,
                                     const std::map<std::string, std::string>& inputs) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::WorkflowTriggerSchema>();
        if (!inputs.empty())
        {
            std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> api_inputs;
            for (const auto& kv : inputs)
            {
                auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
                any_val->fromJson(web::json::value::string(from_std(kv.second)));
                api_inputs[from_std(kv.first)] = any_val;
            }
            body->setInputs(api_inputs);
        }
        auto result = a->srcAppApiWorkflowsTriggerWorkflow(from_std(workflow_id), body).get();
        if (!result)
            throw CyberwaveError("Trigger workflow returned no data");
        return WorkflowRun(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

WorkflowRun WorkflowManager::trigger_with_json(const std::string& workflow_id, const std::string& inputs_json) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::WorkflowTriggerSchema>();
        if (!inputs_json.empty() && inputs_json != "{}")
        {
            auto parsed = web::json::value::parse(from_std(inputs_json));
            if (parsed.is_object())
            {
                std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> api_inputs;
                for (const auto& kv : parsed.as_object())
                {
                    auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
                    any_val->fromJson(kv.second);
                    api_inputs[kv.first] = any_val;
                }
                body->setInputs(api_inputs);
            }
        }
        auto result = a->srcAppApiWorkflowsTriggerWorkflow(from_std(workflow_id), body).get();
        if (!result)
            throw CyberwaveError("Trigger workflow returned no data");
        return WorkflowRun(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

// -----------------------------------------------------------------------------
// WorkflowRunManager
// -----------------------------------------------------------------------------

WorkflowRunManager::WorkflowRunManager(const Client& client) : client_(client) {}

std::vector<WorkflowRun> WorkflowRunManager::list(const std::string& workflow_id,
                                                  const std::string& status_filter) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        boost::optional<utility::string_t> wf_opt;
        if (!workflow_id.empty())
            wf_opt = from_std(workflow_id);
        boost::optional<utility::string_t> status_opt;
        if (!status_filter.empty())
            status_opt = from_std(status_filter);
        auto vec = a->srcAppApiWorkflowRunsListWorkflowRuns(wf_opt, status_opt).get();
        std::vector<WorkflowRun> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(WorkflowRun(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

WorkflowRun WorkflowRunManager::get(const std::string& run_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiWorkflowRunsGetWorkflowRun(from_std(run_id)).get();
        if (!result)
            throw CyberwaveError("Get workflow run returned no data");
        return WorkflowRun(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

WorkflowRun WorkflowRunManager::cancel(const std::string& run_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiWorkflowRunsCancelWorkflowRun(from_std(run_id)).get();
        if (!result)
            throw CyberwaveError("Cancel workflow run returned no data");
        return WorkflowRun(client_, std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
