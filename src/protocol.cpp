#include "protocol.hpp"

#include <regex>

#include <boost/algorithm/string/join.hpp>

#include <xbt.h>

#include <rapidjson/stringbuffer.h>

#include "batsim.hpp"
#include "context.hpp"
#include "jobs.hpp"

using namespace rapidjson;
using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(protocol, "protocol"); //!< Logging

namespace protocol
{

/**
 * @brief Computes the KillProgress of a BatTask
 * @param[in] The task whose kill progress must be computed
 * @return The KillProgress of the given BatTask
 */
std::shared_ptr<batprotocol::KillProgress> battask_to_kill_progress(const BatTask * task)
{
    auto kp = batprotocol::KillProgress::make(task->unique_name());

    std::stack<const BatTask *> tasks;
    tasks.push(task);

    while (!tasks.empty())
    {
        auto t = tasks.top();
        tasks.pop();

        switch (t->profile->type)
        {
        case ProfileType::PTASK: // missing breaks are not a mistake.
        case ProfileType::PTASK_HOMOGENEOUS:
        case ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS:
        case ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES:
        { // Profile is a parallel task.
            double task_progress_ratio = 0;
            if (t->ptask != nullptr) // The parallel task has already started
            {
                // WARNING: 'get_remaining_ratio' is not returning the flops amount but the remaining quantity of work
                // from 1 (not started yet) to 0 (completely finished)
                task_progress_ratio = 1 - t->ptask->get_remaining_ratio();
            }
            kp->add_atomic(t->unique_name(), t->profile->name, task_progress_ratio);
        } break;
        case ProfileType::DELAY:
        {
            double task_progress_ratio = 1;
            if (t->delay_task_required != 0)
            {
                xbt_assert(t->delay_task_start != -1, "Internal error");
                double runtime = simgrid::s4u::Engine::get_clock() - t->delay_task_start;
                task_progress_ratio = runtime / t->delay_task_required;
            }

            kp->add_atomic(t->unique_name(), t->profile->name, task_progress_ratio);
        } break;
        case ProfileType::REPLAY_SMPI: {
            kp->add_atomic(t->unique_name(), t->profile->name, -1);
        } break;
        case ProfileType::SEQUENTIAL_COMPOSITION: {
            xbt_assert(t->sub_tasks.size() == 1, "Internal error");
            auto sub_task = t->sub_tasks[0];
            tasks.push(sub_task);
            xbt_assert(sub_task != nullptr, "Internal error");

            kp->add_sequential(t->unique_name(), t->profile->name, t->current_repetition, t->current_task_index, sub_task->unique_name());
        } break;
        default:
            xbt_die("Unimplemented kill progress of profile type %d", (int)task->profile->type);
        }
    }

    return kp;
}

/**
 * @brief Create a batprotocol::Job from a Batsim Job
 * @return The corresponding batprotocol::Job
 */
std::shared_ptr<batprotocol::Job> to_job(const Job & job)
{
    auto proto_job = batprotocol::Job::make();
    proto_job->set_resource_number(job.requested_nb_res);
    proto_job->set_walltime(job.walltime);
    proto_job->set_profile(job.profile->name); // TODO: handle ghost jobs without profile
    proto_job->set_extra_data(job.extra_data);
    // TODO: handle job rigidity

    return proto_job;
}

/**
 * @brief Returns a batprotocol::fb::FinalJobState corresponding to a given Batsim JobState
 * @param[in] state The Batsim JobState
 * @return A batprotocol::fb::FinalJobState corresponding to a given Batsim JobState
 */
batprotocol::fb::FinalJobState job_state_to_final_job_state(const JobState & state)
{
    using namespace batprotocol;

    switch (state)
    {
    case JobState::JOB_STATE_COMPLETED_SUCCESSFULLY:
        return fb::FinalJobState_COMPLETED_SUCCESSFULLY;
        break;
    case JobState::JOB_STATE_COMPLETED_FAILED:
        return fb::FinalJobState_COMPLETED_FAILED;
        break;
    case JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED:
        return fb::FinalJobState_COMPLETED_WALLTIME_REACHED;
        break;
    case JobState::JOB_STATE_COMPLETED_KILLED:
        return fb::FinalJobState_COMPLETED_KILLED;
        break;
    case JobState::JOB_STATE_REJECTED:
        return fb::FinalJobState_REJECTED;
        break;
    default:
        xbt_assert(false, "Invalid (non-final) job state received: %d", static_cast<int>(state));
    }
}

Periodic from_periodic(const batprotocol::fb::Periodic * periodic)
{
    Periodic p;

    p.period = periodic->period();
    p.offset = periodic->offset();
    p.time_unit = periodic->time_unit();
    switch (periodic->mode_type()) {
        case batprotocol::fb::PeriodicMode_NONE: {
            xbt_assert(false, "invalid periodic received: periodic mode is NONE");
        }
        case batprotocol::fb::PeriodicMode_Infinite: {
            p.is_infinite = true;
        } break;
        case batprotocol::fb::PeriodicMode_FinitePeriodNumber: {
            p.is_infinite = false;
            p.nb_periods = periodic->mode_as_FinitePeriodNumber()->nb_periods();
        } break;
    }

    return p;
}

batprotocol::SimulationBegins to_simulation_begins(const BatsimContext * context)
{
    using namespace batprotocol;

    SimulationBegins begins;

    // Hosts
    begins.set_host_number(context->machines.nb_machines());
    for (const Machine * machine : context->machines.compute_machines())
    {
        auto host = machine->host;
        begins.add_host(machine->id, machine->name, host->get_pstate(), host->get_pstate_count(), fb::HostState_IDLE, host->get_core_count(), machine->pstate_speeds());

        for (auto & kv : machine->properties)
        {
            begins.set_host_property(machine->id, kv.first, kv.second);
        }

        for (auto & kv : machine->zone_properties)
        {
            begins.set_host_zone_property(machine->id, kv.first, kv.second);
        }
    }

    for (const Machine * machine : context->machines.storage_machines())
    {
        auto host = machine->host;
        begins.add_host(machine->id, machine->name, host->get_pstate(), host->get_pstate_count(), fb::HostState_IDLE, host->get_core_count(), machine->pstate_speeds());
        begins.set_host_as_storage(machine->id);

        for (auto & kv : machine->properties)
        {
            begins.set_host_property(machine->id, kv.first, kv.second);
        }

        for (auto & kv : machine->zone_properties)
        {
            begins.set_host_zone_property(machine->id, kv.first, kv.second);
        }
    }

    // Workloads
    for (const auto & kv : context->workloads.workloads())
    {
        const auto & workload_name = kv.first;
        const auto * workload = kv.second;

        begins.add_workload(workload_name, workload->file);

        // TODO: add profiles if requested by the user
    }

    // Misc.
    begins.set_batsim_execution_context(context->main_args->generate_execution_context_json());
    begins.set_batsim_arguments(std::make_shared<std::vector<std::string> >(context->main_args->raw_argv));

    return begins;
}

ExecuteJobMessage * from_execute_job(const batprotocol::fb::ExecuteJobEvent * execute_job, BatsimContext * context)
{
    auto * msg = new ExecuteJobMessage;

    // Retrieve job
    JobIdentifier job_id(execute_job->job_id()->str());
    msg->job = context->workloads.job_at(job_id);

    // Build main job's allocation
    msg->job_allocation = std::make_shared<AllocationPlacement>();
    msg->job_allocation->hosts = IntervalSet::from_string_hyphen(execute_job->allocation()->host_allocation()->str());

    using namespace batprotocol::fb;
    switch (execute_job->allocation()->executor_placement_type())
    {
    case ExecutorPlacement_NONE: {
        xbt_assert(false, "invalid ExecuteJob received: executor placement type of job's main allocation is NONE");
    } break;
    case ExecutorPlacement_PredefinedExecutorPlacementStrategyWrapper: {
        msg->job_allocation->use_predefined_strategy = true;
        msg->job_allocation->predefined_strategy = execute_job->allocation()->executor_placement_as_PredefinedExecutorPlacementStrategyWrapper()->strategy();
    } break;
    case ExecutorPlacement_CustomExecutorToHostMapping: {
        msg->job_allocation->use_predefined_strategy = false;
        auto custom_mapping = execute_job->allocation()->executor_placement_as_CustomExecutorToHostMapping()->mapping();
        msg->job_allocation->custom_mapping.reserve(custom_mapping->size());
        for (unsigned int i = 0; i < custom_mapping->size(); ++i)
        {
            msg->job_allocation->custom_mapping[i] = custom_mapping->Get(i);
        }
    } break;
    }

    // Build override allocations for profiles
    for (unsigned int i = 0; i < execute_job->profile_allocation_override()->size(); ++i)
    {
        auto override_alloc = std::make_shared<::AllocationPlacement>();
        auto override = execute_job->profile_allocation_override()->Get(i);
        auto profile_name = override->profile_id()->str();
        msg->profile_allocation_override[profile_name] = override_alloc;

        override_alloc->hosts = IntervalSet::from_string_hyphen(override->host_allocation()->str());

        switch (override->executor_placement_type())
        {
        case ExecutorPlacement_NONE: {
            xbt_assert(false, "invalid ExecuteJob received: executor placement type of job's main allocation is NONE");
        } break;
        case ExecutorPlacement_PredefinedExecutorPlacementStrategyWrapper: {
            override_alloc->use_predefined_strategy = true;
            override_alloc->predefined_strategy = execute_job->allocation()->executor_placement_as_PredefinedExecutorPlacementStrategyWrapper()->strategy();
        } break;
        case ExecutorPlacement_CustomExecutorToHostMapping: {
            override_alloc->use_predefined_strategy = false;
            auto custom_mapping = execute_job->allocation()->executor_placement_as_CustomExecutorToHostMapping()->mapping();
            override_alloc->custom_mapping.reserve(custom_mapping->size());
            for (unsigned int i = 0; i < custom_mapping->size(); ++i)
            {
                override_alloc->custom_mapping[i] = custom_mapping->Get(i);
            }
        } break;
        }
    }

    // Storage overrides
    for (unsigned int i = 0; i < execute_job->storage_placement()->size(); ++i)
    {
        auto storage_placement = execute_job->storage_placement()->Get(i);
        msg->storage_mapping[storage_placement->storage_name()->str()] = storage_placement->host_id();
    }

    return msg;
}

RejectJobMessage *from_reject_job(const batprotocol::fb::RejectJobEvent * reject_job, BatsimContext * context)
{
    auto msg = new RejectJobMessage;

    JobIdentifier job_id(reject_job->job_id()->str());
    msg->job = context->workloads.job_at(job_id);

    return msg;
}

KillJobsMessage *from_kill_jobs(const batprotocol::fb::KillJobsEvent * kill_jobs, BatsimContext * context)
{
    auto msg = new KillJobsMessage;

    msg->job_ids.reserve(kill_jobs->job_ids()->size());
    msg->jobs.reserve(kill_jobs->job_ids()->size());
    for (unsigned int i = 0; i < kill_jobs->job_ids()->size(); ++i)
    {
        JobIdentifier job_id(kill_jobs->job_ids()->Get(i)->str());
        msg->job_ids.push_back(job_id.to_string());
        msg->jobs.push_back(context->workloads.job_at(job_id));
    }

    return msg;
}

EDCHelloMessage *from_edc_hello(const batprotocol::fb::EDCHelloEvent * edc_hello, BatsimContext * context)
{
    auto msg = new EDCHelloMessage;

    msg->batprotocol_version = edc_hello->batprotocol_version()->str();
    msg->edc_name = edc_hello->decision_component_name()->str();
    msg->edc_version = edc_hello->decision_component_version()->str();
    msg->edc_commit = edc_hello->decision_component_commit()->str();

    msg->requested_simulation_features.dynamic_registration = edc_hello->requested_simulation_features()->dynamic_registration();
    msg->requested_simulation_features.profile_reuse = edc_hello->requested_simulation_features()->profile_reuse();
    msg->requested_simulation_features.acknowledge_dynamic_jobs = edc_hello->requested_simulation_features()->acknowledge_dynamic_jobs();
    msg->requested_simulation_features.forward_profiles_on_job_submission = edc_hello->requested_simulation_features()->forward_profiles_on_job_submission();
    msg->requested_simulation_features.forward_profiles_on_jobs_killed = edc_hello->requested_simulation_features()->forward_profiles_on_jobs_killed();
    msg->requested_simulation_features.forward_profiles_on_simulation_begins = edc_hello->requested_simulation_features()->forward_profiles_on_simulation_begins();
    msg->requested_simulation_features.forward_unknown_external_events = edc_hello->requested_simulation_features()->forward_unknown_external_events();

    return msg;
}

CallMeLaterMessage * from_call_me_later(const batprotocol::fb::CallMeLaterEvent * call_me_later, BatsimContext * context)
{
    auto msg = new CallMeLaterMessage;

    msg->call_id = call_me_later->call_me_later_id()->str();
    switch(call_me_later->when_type()) {
        case batprotocol::fb::TemporalTrigger_NONE: {
            xbt_assert(false, "invalid CallMeLater received: temporal trigger is NONE");
        } break;
        case batprotocol::fb::TemporalTrigger_OneShot: {
            msg->is_periodic = false;
            auto * when = call_me_later->when_as_OneShot();
            msg->target_time = when->time();
            msg->time_unit = when->time_unit();
        } break;
        case batprotocol::fb::TemporalTrigger_Periodic: {
            msg->is_periodic = true;
            msg->periodic = from_periodic(call_me_later->when_as_Periodic());
        } break;
    }

    return msg;
}

StopCallMeLaterMessage * from_stop_call_me_later(const batprotocol::fb::StopCallMeLaterEvent * stop_call_me_later, BatsimContext * context)
{
    auto msg = new StopCallMeLaterMessage;
    msg->call_id = stop_call_me_later->call_me_later_id()->str();

    return msg;
}

CreateProbeMessage * from_create_probe(const batprotocol::fb::CreateProbeEvent * create_probe, BatsimContext * context)
{
    auto msg = new CreateProbeMessage;
    msg->probe_id = create_probe->probe_id()->str();

    // Metrics
    msg->metrics = create_probe->metrics();

    // Resources
    msg->resource_type = create_probe->resources_type();
    switch (create_probe->resources_type()) {
        case batprotocol::fb::Resources_NONE: {
            xbt_assert(false, "invalid CreateProbe received: resource type is NONE");
        } break;
        case batprotocol::fb::Resources_HostResources: {
            auto * host_resources = create_probe->resources_as_HostResources();
            msg->hosts = IntervalSet::from_string_hyphen(host_resources->host_ids()->str());
        } break;
        case batprotocol::fb::Resources_LinkResources: {
            auto * link_resources = create_probe->resources_as_LinkResources();
            auto * links = link_resources->link_ids();
            unsigned int nb_links = links->size();
            msg->links.reserve(nb_links);
            for (unsigned int i = 0; i < nb_links; ++i) {
                msg->links.emplace_back(links->Get(i)->str());
            }
        } break;
    }

    // Measurement triggering policy
    msg->measurement_triggering_policy = create_probe->measurement_triggering_policy_type();
    switch (msg->measurement_triggering_policy) {
        case batprotocol::fb::ProbeMeasurementTriggeringPolicy_NONE: {
            xbt_assert(false, "invalid CreateProbe received: measurement triggering policy is NONE");
        } break;
        case batprotocol::fb::ProbeMeasurementTriggeringPolicy_TemporalTriggerWrapper: {
            auto * temporal_trigger_wrapper = create_probe->measurement_triggering_policy_as_TemporalTriggerWrapper();
            switch(temporal_trigger_wrapper->temporal_trigger_type()) {
                case batprotocol::fb::TemporalTrigger_NONE: {
                    xbt_assert(false, "invalid CreateProbe received: temporal trigger is NONE");
                } break;
                case batprotocol::fb::TemporalTrigger_OneShot: {
                    msg->is_periodic = false;
                    auto * temporal_trigger = temporal_trigger_wrapper->temporal_trigger_as_OneShot();
                    msg->target_time = temporal_trigger->time();
                    msg->time_unit = temporal_trigger->time_unit();
                } break;
                case batprotocol::fb::TemporalTrigger_Periodic: {
                    msg->is_periodic = true;
                    msg->periodic = from_periodic(temporal_trigger_wrapper->temporal_trigger_as_Periodic());
                } break;
            }
        } break;
    }

    // Data accumulation strategy
    msg->data_accumulation_strategy = create_probe->data_accumulation_strategy_type();
    switch (create_probe->data_accumulation_strategy_type()) {
        case batprotocol::fb::ProbeDataAccumulationStrategy_NONE: {
            xbt_assert(false, "invalid CreateProbe received: data accumulation strategy is NONE");
        } break;
        case batprotocol::fb::ProbeDataAccumulationStrategy_NoProbeDataAccumulation: {
        } break;
        case batprotocol::fb::ProbeDataAccumulationStrategy_ProbeDataAccumulation: {
            auto * accumulation = create_probe->data_accumulation_strategy_as_ProbeDataAccumulation();

            msg->data_accumulation_reset_mode = accumulation->reset_mode_type();
            switch (accumulation->reset_mode_type()) {
                case batprotocol::fb::ResetMode_NONE: {
                    xbt_assert(false, "invalid CreateProbe received: data accumulation strategy's reset mode is NONE");
                } break;
                case batprotocol::fb::ResetMode_NoReset: {
                } break;
                case batprotocol::fb::ResetMode_ProbeAccumulationReset: {
                    msg->data_accumulation_reset_value = accumulation->reset_mode_as_ProbeAccumulationReset()->new_value();
                } break;
            }

            msg->data_accumulation_cumulative_function = accumulation->cumulative_function();
            msg->data_accumulation_temporal_normalization = accumulation->temporal_normalization();
        } break;
    }

    // Resource aggregation
    msg->resource_agregation_type = create_probe->resources_aggregation_function_type();
    switch (create_probe->resources_aggregation_function_type()) {
        case batprotocol::fb::ResourcesAggregationFunction_NONE: {
            xbt_assert(false, "invalid CreateProbe received: resource aggregation function is NONE");
        } break;
        case batprotocol::fb::ResourcesAggregationFunction_NoResourcesAggregation: {
        } break;
        case batprotocol::fb::ResourcesAggregationFunction_Sum: {
        } break;
        case batprotocol::fb::ResourcesAggregationFunction_ArithmeticMean: {
        } break;
        case batprotocol::fb::ResourcesAggregationFunction_QuantileFunction: {
            msg->quantile_threshold = create_probe->resources_aggregation_function_as_QuantileFunction()->threshold();
        } break;
    }

    // Temporal aggregation
    msg->temporal_aggregation_type = create_probe->temporal_aggregation_function_type();

    // Emission filtering policy
    msg->emission_filtering_policy = create_probe->emission_filtering_policy_type();
    switch (create_probe->emission_filtering_policy_type()) {
        case batprotocol::fb::ProbeEmissionFilteringPolicy_NONE: {
            xbt_assert(false, "invalid CreateProbe received: emission filtering policy is NONE");
        } break;
        case batprotocol::fb::ProbeEmissionFilteringPolicy_NoFiltering: {
        } break;
        case batprotocol::fb::ProbeEmissionFilteringPolicy_ThresholdFilteringFunction: {
            auto * filtering_function = create_probe->emission_filtering_policy_as_ThresholdFilteringFunction();
            msg->emission_filtering_threshold_value = filtering_function->threshold();
            msg->emission_filtering_threshold_comparator = filtering_function->operator_();
        } break;
    }

    return msg;
}

StopProbeMessage * from_stop_probe(const batprotocol::fb::StopProbeEvent * stop_probe, BatsimContext * context)
{
    auto msg = new StopProbeMessage;
    msg->probe_id = stop_probe->probe_id()->str();

    return msg;
}

void parse_batprotocol_message(const uint8_t * buffer, uint32_t buffer_size, double & now, std::shared_ptr<std::vector<IPMessageWithTimestamp> > & messages, BatsimContext * context)
{
    (void) buffer_size;
    auto parsed = batprotocol::deserialize_message(*context->proto_msg_builder, context->edc_json_format, buffer);
    now = parsed->now();
    messages->resize(parsed->events()->size());

    double preceding_event_timestamp = -1;
    if (parsed->events()->size() > 0)
        preceding_event_timestamp = parsed->events()->Get(0)->timestamp();

    for (unsigned int i = 0; i < parsed->events()->size(); ++i)
    {
        auto event_timestamp = parsed->events()->Get(i);
        auto ip_message = new IPMessage;

        messages->at(i).timestamp = event_timestamp->timestamp();
        messages->at(i).message = ip_message;

        xbt_assert(messages->at(i).timestamp <= now,
            "invalid event %u (type='%s') in message: event timestamp (%g) is after message's now (%g)",
            i, batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()], messages->at(i).timestamp, now
        );
        xbt_assert(messages->at(i).timestamp >= preceding_event_timestamp,
            "invalid event %u (type='%s') in message: event timestamp (%g) is before preceding event's timestamp (%g) while events should be in chronological order",
            i, batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()], messages->at(i).timestamp, preceding_event_timestamp
        );

        XBT_INFO("Parsing an event of type=%s", batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()]);
        using namespace batprotocol::fb;
        switch (event_timestamp->event_type())
        {
        case Event_EDCHelloEvent: {
            ip_message->type = IPMessageType::SCHED_HELLO;
            ip_message->data = static_cast<void *>(from_edc_hello(event_timestamp->event_as_EDCHelloEvent(), context));
        } break;
        case Event_ExecuteJobEvent: {
            ip_message->type = IPMessageType::SCHED_EXECUTE_JOB;
            ip_message->data = static_cast<void *>(from_execute_job(event_timestamp->event_as_ExecuteJobEvent(), context));
        } break;
        case Event_RejectJobEvent: {
            ip_message->type = IPMessageType::SCHED_REJECT_JOB;
            ip_message->data = static_cast<void *>(from_reject_job(event_timestamp->event_as_RejectJobEvent(), context));
        } break;
        case Event_KillJobsEvent: {
            ip_message->type = IPMessageType::SCHED_KILL_JOBS;
            ip_message->data = static_cast<void *>(from_kill_jobs(event_timestamp->event_as_KillJobsEvent(), context));
        } break;
        case Event_CallMeLaterEvent: {
            ip_message->type = IPMessageType::SCHED_CALL_ME_LATER;
            ip_message->data = static_cast<void *>(from_call_me_later(event_timestamp->event_as_CallMeLaterEvent(), context));
        } break;
        case Event_StopCallMeLaterEvent: {
            ip_message->type = IPMessageType::SCHED_STOP_CALL_ME_LATER;
            ip_message->data = static_cast<void *>(from_stop_call_me_later(event_timestamp->event_as_StopCallMeLaterEvent(), context));
        } break;
        case Event_CreateProbeEvent: {
            ip_message->type = IPMessageType::SCHED_CREATE_PROBE;
            ip_message->data = static_cast<void *>(from_create_probe(event_timestamp->event_as_CreateProbeEvent(), context));
        } break;
        case Event_StopProbeEvent: {
            ip_message->type = IPMessageType::SCHED_STOP_PROBE;
            ip_message->data = static_cast<void *>(from_stop_probe(event_timestamp->event_as_StopProbeEvent(), context));
        } break;
        case Event_RegisterJobEvent: {
            ip_message->type = IPMessageType::SCHED_JOB_REGISTERED;
            ip_message->data = static_cast<void *>(from_register_job(event_timestamp->event_as_RegisterJobEvent(), context));
        } break;
        case Event_RegisterProfileEvent: {
            ip_message->type = IPMessageType::SCHED_PROFILE_REGISTERED;
            ip_message->data = static_cast<void *>(from_register_profile(event_timestamp->event_as_RegisterProfileEvent(), context));
        } break;
        case Event_FinishRegistrationEvent: {
            ip_message->type = IPMessageType::SCHED_END_DYNAMIC_REGISTRATION;
            // No data in this event
        } break;
        default: {
            xbt_assert(false, "Unhandled event type received (%s)", batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()]);
        } break;
        }
    }
}

} // end of namespace protocol
