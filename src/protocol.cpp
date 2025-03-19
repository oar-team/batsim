#include "protocol.hpp"

#include <regex>
#include <filesystem>

//#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

#include <xbt.h>

#include <rapidjson/stringbuffer.h>

#include "batsim.hpp"
#include "context.hpp"

using namespace rapidjson;
using namespace std;
namespace fs = std::filesystem;

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
    proto_job->set_profile(job.profile->name);
    proto_job->set_extra_data(job.extra_data);

    return proto_job;
}



std::shared_ptr<batprotocol::Profile> to_profile(const Profile & profile)
{
    std::shared_ptr<batprotocol::Profile> p;
    switch(profile.type)
    {
    case ProfileType::DELAY:
    {
        auto * data = static_cast<DelayProfileData*>(profile.data);
        p = batprotocol::Profile::make_delay(data->delay);
        break;
    }
    case ProfileType::PTASK:
    {
        auto * data = static_cast<ParallelProfileData*>(profile.data);

        const std::shared_ptr<std::vector<double>> cpu_vector = make_shared<vector<double>>(vector<double>());
        cpu_vector->reserve(data->nb_res);
        cpu_vector->assign(data->cpu, data->cpu+data->nb_res);

        const std::shared_ptr<std::vector<double>> comm_vector = make_shared<vector<double>>(vector<double>());
        comm_vector->reserve(data->nb_res*data->nb_res);
        comm_vector->assign(data->com, data->com+data->nb_res*data->nb_res);

        p = batprotocol::Profile::make_parallel_task(cpu_vector, comm_vector);
        break;
    }
    case ProfileType::PTASK_HOMOGENEOUS:
    {
        auto * data = static_cast<ParallelHomogeneousProfileData*>(profile.data);
        p = batprotocol::Profile::make_parallel_task_homogeneous(data->strategy, data->cpu, data->com);
        break;
    }
    case ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS:
    {
        auto * data = static_cast<ParallelTaskOnStorageHomogeneousProfileData*>(profile.data);
        p = batprotocol::Profile::make_parallel_task_on_storage_homogeneous(data->storage_label,
                                                                            data->strategy,
                                                                            data->bytes_to_read, data->bytes_to_write);
        break;
    }
    case ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES:
    {
        auto * data = static_cast<DataStagingProfileData*>(profile.data);
        p = batprotocol::Profile::make_parallel_task_data_staging_between_storages(data->nb_bytes,
                                                                                   data->from_storage_label,
                                                                                   data->to_storage_label);
        break;
    }
    case ProfileType::PTASK_MERGE_COMPOSITION:
    {
        auto * data = static_cast<ParallelTaskMergeCompositionProfileData*>(profile.data);
        const std::shared_ptr<std::vector<std::string>> sub_profiles = make_shared<vector<std::string>>(data->profile_names);
        p = batprotocol::Profile::make_parallel_task_merge_composition(sub_profiles);
        break;
    }
    case ProfileType::SEQUENTIAL_COMPOSITION:
    {
        auto * data = static_cast<SequenceProfileData*>(profile.data);
        const std::shared_ptr<std::vector<std::string>> sub_profiles = make_shared<vector<std::string>>(data->sequence_names);
        p = batprotocol::Profile::make_sequential_composition(sub_profiles, data->repetition_count);
        break;
    }
    case ProfileType::FORKJOIN_COMPOSITION:
    {
        auto * data = static_cast<ForkJoinCompositionProfileData*>(profile.data);
        const std::shared_ptr<std::vector<std::string>> sub_profiles = make_shared<vector<std::string>>(data->profile_names);
        p = batprotocol::Profile::make_forkjoin_composition(sub_profiles);
        break;
    }
    case ProfileType::REPLAY_SMPI:
    {
        auto * data = static_cast<TraceReplayProfileData*>(profile.data);
        p = batprotocol::Profile::make_trace_replay_smpi(data->filename);
        break;
    }
    case ProfileType::REPLAY_USAGE:
    {
        auto * data = static_cast<TraceReplayProfileData*>(profile.data);
        p = batprotocol::Profile::make_trace_replay_fractional_computation(data->filename);
        break;
    }
    default:
        xbt_die("Invalid profile type");
    }

    if (!profile.extra_data.empty())
    {
        p->set_extra_data(profile.extra_data);
    }

    return p;
}


std::shared_ptr<batprotocol::ExternalEvent> to_external_event(const ExternalEvent & external_event)
{
    std::shared_ptr<batprotocol::ExternalEvent> e;
    switch(external_event.type)
    {
    case ExternalEventType::GENERIC:
    default: // Treat Unknow event types as generic. TODO change this?
        GenericEventData * event_data = static_cast<GenericEventData*>(external_event.data);
        e = batprotocol::ExternalEvent::make_generic_event(event_data->json_desc_str);
    }
    return e;
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

        // Add profiles if requested by the EDC
        if (context->forward_profiles_on_simulation_begins)
        {
            for (const auto & kv : workload->profiles->profiles())
            {
                std::shared_ptr<batprotocol::Profile> proto_profile = to_profile(*kv.second);
                begins.add_profile(kv.first, proto_profile);
            }
        }
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
    msg->job_id = JobIdentifier(execute_job->job_id()->str());

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

    msg->job_id = JobIdentifier(reject_job->job_id()->str());

    return msg;
}

KillJobsMessage *from_kill_jobs(const batprotocol::fb::KillJobsEvent * kill_jobs, BatsimContext * context)
{
    auto msg = new KillJobsMessage;
    auto * jobs_list = kill_jobs->job_ids();
    msg->job_ids.reserve(jobs_list->size());
    for (unsigned int i = 0; i < jobs_list->size(); ++i)
    {
        msg->job_ids.push_back(JobIdentifier(jobs_list->Get(i)->str()));
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


JobRegisteredByEDCMessage * from_register_job(const batprotocol::fb::RegisterJobEvent * register_job, BatsimContext * context)
{
    auto msg = new JobRegisteredByEDCMessage;

    const batprotocol::fb::Job * proto_job = register_job->job();

    msg->job = std::make_shared<Job>();
    msg->job->id = JobIdentifier(register_job->job_id()->str());
    msg->job->requested_nb_res = proto_job->resource_request();
    msg->job->walltime = proto_job->walltime();

    if (proto_job->extra_data() != nullptr)
    {
        msg->job->extra_data = proto_job->extra_data()->str();
    }
    msg->profile_id = proto_job->profile_id()->str();

    return msg;
}


ProfileRegisteredByEDCMessage * from_register_profile(const batprotocol::fb::RegisterProfileEvent * register_profile, BatsimContext * context)
{
    const batprotocol::fb::ProfileAndId * proto_profile = register_profile->profile();

    auto msg = new ProfileRegisteredByEDCMessage;
    ProfilePtr profile = std::make_shared<Profile>();
    msg->profile = profile;
    msg->profile_id = proto_profile->id()->str();

    if (proto_profile->extra_data() != nullptr)
    {
        msg->profile->extra_data = proto_profile->extra_data()->str();
    }

    switch(proto_profile->profile_type())
    {
    case batprotocol::fb::Profile_DelayProfile:
    {
        const batprotocol::fb::DelayProfile * prof = proto_profile->profile_as_DelayProfile();
        profile->type = ProfileType::DELAY;
        DelayProfileData * data = new DelayProfileData;
        data->delay = prof->delay();

        xbt_assert(data->delay > 0, "Invalid registration of profile '%s': non-strictly-positive 'delay' field (%g)",
                   msg->profile_id.c_str(), data->delay);

        profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_ParallelTaskProfile:
    {
        const batprotocol::fb::ParallelTaskProfile * prof = proto_profile->profile_as_ParallelTaskProfile();
        profile->type = ProfileType::PTASK;
        ParallelProfileData * data = new ParallelProfileData;

        auto * cpu_vector = prof->computation_vector();
        unsigned int nb_res = cpu_vector->size();

        data->nb_res = nb_res;
        data->cpu = new double[nb_res];
        for (unsigned int i = 0; i < nb_res; ++i)
        {
            data->cpu[i] = cpu_vector->Get(i);

            xbt_assert(data->cpu[i] >= 0, "Invalid registration of profile '%s': elements of 'computation_vector' must be non-negative (%g)",
                       msg->profile_id.c_str(), data->cpu[i]);
        }

        unsigned int com_size = nb_res * nb_res;
        auto * com_vector = prof->communication_matrix();

        xbt_assert(com_size == com_vector->size(),
                   "Invalid registration of profile '%s': incoherent communication_matrix of size %d, expected size is %d",
                   msg->profile_id.c_str(), com_vector->size(), com_size);

        data->com = new double[com_size];
        for (unsigned int i = 0; i < com_size; ++i)
        {
            data->com[i] = com_vector->Get(i);

            xbt_assert(data->com[i] >= 0, "Invalid registration of profile '%s': elements of 'communication_matrix' must be non-negative (%g)",
                       msg->profile_id.c_str(), data->com[i]);
        }

        profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_ParallelTaskHomogeneousProfile:
    {
        const batprotocol::fb::ParallelTaskHomogeneousProfile * prof = proto_profile->profile_as_ParallelTaskHomogeneousProfile();
        profile->type = ProfileType::PTASK_HOMOGENEOUS;
        ParallelHomogeneousProfileData * data = new ParallelHomogeneousProfileData;

        data->cpu = prof->computation_amount();
        data->com = prof->communication_amount();
        data->strategy = prof->generation_strategy(); // The diferent strategies are handled in task_execution

        xbt_assert(data->cpu >= 0, "Invalid registration of profile '%s': 'computation_amount' must be non-negative (%g)",
                   msg->profile_id.c_str(), data->cpu);
        xbt_assert(data->com >= 0, "Invalid registration of profile '%s': 'communication_amount' must be non-negative (%g)",
                   msg->profile_id.c_str(), data->com);

        profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_SequentialCompositionProfile:
    {
        const batprotocol::fb::SequentialCompositionProfile * prof = proto_profile->profile_as_SequentialCompositionProfile();
        profile->type = ProfileType::SEQUENTIAL_COMPOSITION;
        SequenceProfileData * data = new SequenceProfileData;

        data->repetition_count = prof->repetition_count();

        auto * ids_vector = prof->profile_ids();
        data->sequence_names.reserve(ids_vector->size());

        for (unsigned int i = 0; i < ids_vector->size(); ++i)
        {
            data->sequence_names.push_back(ids_vector->Get(i)->str());
        }

        profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_ForkJoinCompositionProfile:
    {
        // const batprotocol::fb::ForkJoinCompositionProfile * prof = proto_profile->profile_as_ForkJoinCompositionProfile();
        // profile->type = ProfileType::FORKJOIN_COMPOSITION;
        // ParallelProfileData * data = new ParallelProfileData;

        xbt_die("Handling of profile ForkJoin Composition is not implemented yet");

        //TODO
        //profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_ParallelTaskMergeCompositionProfile:
    {
        // const batprotocol::fb::ParallelTaskMergeCompositionProfile * prof = proto_profile->profile_as_ParallelTaskMergeCompositionProfile();
        // profile->type = ProfileType::PTASK_MERGE_COMPOSITION;
        // ParallelProfileData * data = new ParallelProfileData;

        xbt_die("Handling of profile Parallel Task Merge Composition is not implemented yet");

        //TODO
        // profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_ParallelTaskOnStorageHomogeneousProfile:
    {
        const batprotocol::fb::ParallelTaskOnStorageHomogeneousProfile * prof = proto_profile->profile_as_ParallelTaskOnStorageHomogeneousProfile();
        profile->type = ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS;
        ParallelTaskOnStorageHomogeneousProfileData * data = new ParallelTaskOnStorageHomogeneousProfileData;

        data->bytes_to_read = prof->bytes_to_read();
        data->bytes_to_write = prof->bytes_to_write();

        xbt_assert(data->bytes_to_read >= 0,
                   "Invalid registration of profile '%s': non-positive 'bytes_to_read' (%g)",
                   msg->profile_id.c_str(), data->bytes_to_read);
        xbt_assert(data->bytes_to_write >= 0,
                   "Invalid registration of profile '%s': non-positive 'bytes_to_write' (%g)",
                   msg->profile_id.c_str(), data->bytes_to_write);

        data->storage_label = prof->storage_name()->str();
        data->strategy = prof->generation_strategy(); // The diferent strategies are handled in task_execution

        profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_ParallelTaskDataStagingBetweenStoragesProfile:
    {
        const batprotocol::fb::ParallelTaskDataStagingBetweenStoragesProfile * prof = proto_profile->profile_as_ParallelTaskDataStagingBetweenStoragesProfile();
        profile->type = ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES;
        DataStagingProfileData * data = new DataStagingProfileData;

        data->nb_bytes = prof->bytes_to_transfer();
        data->from_storage_label = prof->emitter_storage_name()->str();
        data->to_storage_label = prof->receiver_storage_name()->str();

        profile->data = data;
        break;
    }
    case batprotocol::fb::Profile_TraceReplayProfile:
    {
        const batprotocol::fb::TraceReplayProfile * prof = proto_profile->profile_as_TraceReplayProfile();
        TraceReplayProfileData * data = new TraceReplayProfileData;
        data->filename = prof->filename()->str();

        fs::path trace_path(data->filename);
        xbt_assert(fs::exists(trace_path) && fs::is_regular_file(trace_path),
                   "Invalid registration of profile '%s': invalid 'trace_file' field ('%s') which leads to a non-existent file ('%s')",
                   msg->profile_id.c_str(), data->filename.c_str(), trace_path.string().c_str());

        ifstream trace_file(trace_path.string());
        xbt_assert(trace_file.is_open(), "Cannot open file '%s'", trace_path.string().c_str());

        // load the list of trace files given in the filenam
        std::vector<std::string> trace_filenames;
        string line;
        while (std::getline(trace_file, line))
        {
            boost::trim_right(line);
            fs::path rank_trace_path = trace_path.parent_path().string() + "/" + line;
            trace_filenames.push_back(rank_trace_path.string());
        }

        XBT_DEBUG("Filenames of profile '%s': [%s]", profile->name.c_str(), boost::algorithm::join(trace_filenames, ", ").c_str());

        data->trace_filenames = trace_filenames;
        profile->data = data;

        if (prof->trace_type() == batprotocol::fb::TraceType_SMPI)
        {
            profile->type = ProfileType::REPLAY_SMPI;

        }
        else if (prof->trace_type() == batprotocol::fb::TraceType_FractionalComputation)
        {
            profile->type = ProfileType::REPLAY_USAGE;
        }
        else
        {
            xbt_assert(false, "Unhandled TraceReplay profile (%s)", batprotocol::fb::EnumNamesTraceType()[prof->trace_type()]);
        }

        break;
    }
    default:
        xbt_die("Unhandled registration of profile type %s", batprotocol::fb::EnumNameProfile(proto_profile->profile_type()));
    }

    return msg;
}


ChangeHostPStateMessage * from_change_host_pstate(const batprotocol::fb::ChangeHostPStateEvent * change_host_pstate, BatsimContext * context) {
    auto msg = new ChangeHostPStateMessage;

    msg->machine_ids = IntervalSet::from_string_hyphen(change_host_pstate->host_ids()->str());
    msg->new_pstate = change_host_pstate->pstate();

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
        case Event_ForceSimulationStopEvent: {
            ip_message->type = IPMessageType::SCHED_FORCE_SIMULATION_STOP;
            // No data in this event
            break;
        }
        case Event_ChangeHostPStateEvent: {
            ip_message->type = IPMessageType::SCHED_CHANGE_HOST_PSTATE;
            ip_message->data = static_cast<void *>(from_change_host_pstate(event_timestamp->event_as_ChangeHostPStateEvent(), context));
            break;
        }
        default: {
            xbt_assert(false, "Unhandled event type received (%s)", batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()]);
        } break;
        }
    }
}

} // end of namespace protocol
