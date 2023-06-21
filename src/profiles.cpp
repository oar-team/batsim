/**
 * @file profiles.cpp
 * @brief Contains profile-related structures and classes
 */

#include "profiles.hpp"

#include <fstream>
#include <iostream>
#include <filesystem>

#include <boost/algorithm/string.hpp>

#include <xbt/asserts.h>

#include <rapidjson/document.h>

using namespace std;
using namespace rapidjson;
namespace fs = std::filesystem;

XBT_LOG_NEW_DEFAULT_CATEGORY(profiles, "profiles"); //!< Logging

Profiles::~Profiles()
{
    _profiles.clear();
}

void Profiles::load_from_json(const Document &doc, const string & filename)
{
    string error_prefix = "Invalid JSON file '" + filename + "'";

    xbt_assert(doc.IsObject(), "%s: not a JSON object", error_prefix.c_str());
    xbt_assert(doc.HasMember("profiles"), "%s: the 'profiles' object is missing",
               error_prefix.c_str());
    const Value & profiles = doc["profiles"];
    xbt_assert(profiles.IsObject(), "%s: the 'profiles' member is not an object",
               error_prefix.c_str());

    for (Value::ConstMemberIterator it = profiles.MemberBegin(); it != profiles.MemberEnd(); ++it)
    {
        const Value & key = it->name;
        const Value & value = it->value;

        xbt_assert(key.IsString(), "%s: all children of the 'profiles' object must have a "
                   "string key", error_prefix.c_str());
        string profile_name = key.GetString();

        auto profile = Profile::from_json(profile_name, value, error_prefix, true, filename);
        xbt_assert(!exists(string(key.GetString())), "%s: duplication of profile name '%s'",
                   error_prefix.c_str(), key.GetString());
        _profiles[string(key.GetString())] = profile;
    }
}

ProfilePtr Profiles::operator[](const std::string &profile_name)
{
    auto mit = _profiles.find(profile_name);
    xbt_assert(mit != _profiles.end(), "Cannot get profile '%s': it does not exist", profile_name.c_str());
    xbt_assert(mit->second.get() != nullptr, "Cannot get profile '%s': it existed some time ago but is no longer accessible", profile_name.c_str());
    return mit->second;
}

const ProfilePtr Profiles::operator[](const std::string &profile_name) const
{
    auto mit = _profiles.find(profile_name);
    xbt_assert(mit != _profiles.end(), "Cannot get profile '%s': it does not exist", profile_name.c_str());
    xbt_assert(mit->second.get() != nullptr, "Cannot get profile '%s': it existed some time ago but is no longer accessible", profile_name.c_str());
    return mit->second;
}

ProfilePtr Profiles::at(const std::string & profile_name)
{
    return operator[](profile_name);
}

const ProfilePtr Profiles::at(const std::string & profile_name) const
{
    return operator[](profile_name);
}

bool Profiles::exists(const std::string &profile_name) const
{
    auto mit = _profiles.find(profile_name);
    return mit != _profiles.end();
}

void Profiles::add_profile(const std::string & profile_name,
                           ProfilePtr & profile)
{
    xbt_assert(!exists(profile_name),
               "Bad Profiles::add_profile call: A profile with name='%s' already exists.",
               profile_name.c_str());

    _profiles[profile_name] = profile;
}

void Profiles::remove_profile(const std::string & profile_name)
{
    auto mit = _profiles.find(profile_name);
    xbt_assert(mit != _profiles.end(), "Bad Profiles::remove_profile call: Profile with name='%s' never existed in this workload.", profile_name.c_str());

    // If the profile has aleady been removed, do nothing.
    if (mit->second.get() == nullptr)
    {
        return;
    }

    // If the profile is composed, also remove links to subprofiles.
    if (mit->second->type == ProfileType::SEQUENTIAL_COMPOSITION)
    {
        auto * profile_data = static_cast<SequenceProfileData*>(mit->second->data);
        for (const auto & subprofile : profile_data->profile_sequence)
        {
            remove_profile(subprofile->name);
        }
    }

    // Discard link to the profile (implicit memory clean-up)
    mit->second = nullptr;
}

void Profiles::remove_unreferenced_profiles()
{
    for (auto & mit : _profiles)
    {
        if (mit.second.use_count() < 2)
        {
            mit.second = nullptr;
        }
    }
}

const std::unordered_map<std::string, ProfilePtr> Profiles::profiles() const
{
    return _profiles;
}

int Profiles::nb_profiles() const
{
    return static_cast<int>(_profiles.size());
}


ParallelProfileData::~ParallelProfileData()
{
    if (cpu != nullptr)
    {
        delete[] cpu;
        cpu = nullptr;
    }
    if (com != nullptr)
    {
        delete[] com;
        com = nullptr;
    }
}

Profile::~Profile()
{
    XBT_INFO("Profile '%s' is being deleted.", name.c_str());
    if (type == ProfileType::DELAY)
    {
        auto * d = static_cast<DelayProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::PTASK)
    {
        auto * d = static_cast<ParallelProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::PTASK_HOMOGENEOUS)
    {
        auto * d = static_cast<ParallelHomogeneousProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::REPLAY_SMPI)
    {
        auto * d = static_cast<ReplaySmpiProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::REPLAY_USAGE)
    {
        auto * d = static_cast<ReplayUsageProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::SEQUENTIAL_COMPOSITION)
    {
        auto * d = static_cast<SequenceProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS)
    {
        auto * d = static_cast<ParallelTaskOnStorageHomogeneousProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES)
    {
        auto * d = static_cast<DataStagingProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::SCHEDULER_SEND)
    {
        auto * d = static_cast<SchedulerSendProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::SCHEDULER_RECV)
    {
        auto * d = static_cast<SchedulerRecvProfileData *>(data);
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else
    {
        XBT_ERROR("Deletion of an unknown profile type (%d)", static_cast<int>(type));
    }
}

// Do NOT remove namespaces in the arguments (to avoid doxygen warnings)
ProfilePtr Profile::from_json(const std::string & profile_name,
                            const rapidjson::Value & json_desc,
                            const std::string & error_prefix,
                            bool is_from_a_file,
                            const std::string & json_filename)
{
    (void) error_prefix; // Avoids a warning if assertions are ignored

    auto profile = std::make_shared<Profile>();
    profile->name = profile_name;

    xbt_assert(json_desc.IsObject(), "%s: profile '%s' value must be an object",
               error_prefix.c_str(), profile_name.c_str());
    xbt_assert(json_desc.HasMember("type"), "%s: profile '%s' has no 'type' field",
               error_prefix.c_str(), profile_name.c_str());
    xbt_assert(json_desc["type"].IsString(), "%s: profile '%s' has a non-string 'type' field",
               error_prefix.c_str(), profile_name.c_str());

    string profile_type = json_desc["type"].GetString();

    int return_code = 0;
    if (json_desc.HasMember("ret"))
    {
        return_code = json_desc["ret"].GetInt();
    }
    profile->return_code = return_code;

    if (profile_type == "delay")
    {
        profile->type = ProfileType::DELAY;
        DelayProfileData * data = new DelayProfileData;

        xbt_assert(json_desc.HasMember("delay"), "%s: profile '%s' has no 'delay' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["delay"].IsNumber(), "%s: profile '%s' has a non-number 'delay' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->delay = json_desc["delay"].GetDouble();

        xbt_assert(data->delay > 0, "%s: profile '%s' has a non-strictly-positive 'delay' field (%g)",
                   error_prefix.c_str(), profile_name.c_str(), data->delay);

        profile->data = data;
    }
    else if (profile_type == "ptask")
    {
        profile->type = ProfileType::PTASK;
        ParallelProfileData * data = new ParallelProfileData;

        // basic checks
        xbt_assert(json_desc.HasMember("cpu"), "%s: profile '%s' has no 'cpu' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc.HasMember("com"), "%s: profile '%s' has no 'com' field",
                   error_prefix.c_str(), profile_name.c_str());

        // get and check CPU vector
        const Value & cpu = json_desc["cpu"];
        xbt_assert(cpu.IsArray(), "%s: profile '%s' has a non-array 'cpu' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(cpu.Size() > 0, "%s: profile '%s' has an invalid-sized array 'cpu' (size=%d): "
                   "must be strictly positive",
                   error_prefix.c_str(), profile_name.c_str(), cpu.Size());

        data->nb_res = cpu.Size();
        data->cpu = new double[data->nb_res];
        for (unsigned int i = 0; i < cpu.Size(); ++i)
        {
            xbt_assert(cpu[i].IsNumber(), "%s: profile '%s' computation array is invalid: all "
                       "elements must be numbers", error_prefix.c_str(), profile_name.c_str());
            data->cpu[i] = cpu[i].GetDouble();
            xbt_assert(data->cpu[i] >= 0, "%s: profile '%s' computation array is invalid: all "
                       "elements must be non-negative", error_prefix.c_str(), profile_name.c_str());
        }

        // get and check Comm vector
        const Value & com = json_desc["com"];
        xbt_assert(com.IsArray(), "%s: profile '%s' has a non-array 'com' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(com.Size() == data->nb_res * data->nb_res, "%s: profile '%s' is incoherent: "
                   "com array has size %d whereas the required array size is %d",
                   error_prefix.c_str(), profile_name.c_str(), com.Size(), data->nb_res * data->nb_res);

        data->com = new double[data->nb_res * data->nb_res];
        for (unsigned int i = 0; i < com.Size(); ++i)
        {
            xbt_assert(com[i].IsNumber(), "%s: profile '%s' communication array is invalid: all "
                       "elements must be numbers", error_prefix.c_str(), profile_name.c_str());
            data->com[i] = com[i].GetDouble();
            xbt_assert(data->com[i] >= 0, "%s: profile '%s' communication array is invalid: all "
                       "elements must be non-negative", error_prefix.c_str(), profile_name.c_str());
        }

        profile->data = data;
    }
    else if (profile_type == "ptask_homogeneous")
    {
        profile->type = ProfileType::PTASK_HOMOGENEOUS;
        ParallelHomogeneousProfileData * data = new ParallelHomogeneousProfileData;

        xbt_assert(json_desc.HasMember("cpu"), "%s: profile '%s' has no 'cpu' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["cpu"].IsNumber(), "%s: profile '%s' has a non-number 'cpu' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->cpu = json_desc["cpu"].GetDouble();
        xbt_assert(data->cpu >= 0, "%s: profile '%s' has a non-positive 'cpu' field (%g)",
                   error_prefix.c_str(), profile_name.c_str(), data->cpu);

        xbt_assert(json_desc.HasMember("com"), "%s: profile '%s' has no 'com' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["com"].IsNumber(), "%s: profile '%s' has a non-number 'com' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->com = json_desc["com"].GetDouble();
        xbt_assert(data->com >= 0, "%s: profile '%s' has a non-positive 'com' field (%g)",
                   error_prefix.c_str(), profile_name.c_str(), data->com);

        auto strategy = batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsUsedForEachValue;
        if (json_desc.HasMember("generation_strategy"))
        {
            xbt_assert(json_desc["generation_strategy"].IsString(), "%s: profile '%s' has a non-string 'generation_strategy' field",
                error_prefix.c_str(), profile_name.c_str());

            std::string strategy_str = json_desc["generation_strategy"].GetString();
            if (strategy_str == "defined_amount_used_for_each_value")
                strategy = batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsUsedForEachValue;
            else if (strategy_str == "defined_amount_spread_uniformly")
                strategy = batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsSpreadUniformly;
        }
        data->strategy = strategy;

        profile->data = data;
    }
    else if (profile_type == "sequential_composition")
    {
        profile->type = ProfileType::SEQUENTIAL_COMPOSITION;
        SequenceProfileData * data = new SequenceProfileData;

        unsigned int repeat = 1;
        if (json_desc.HasMember("repeat"))
        {
            xbt_assert(json_desc["repeat"].IsInt(), "%s: profile '%s' has a non-integral 'repeat' field",
                   error_prefix.c_str(), profile_name.c_str());
            xbt_assert(json_desc["repeat"].GetInt() >= 0, "%s: profile '%s' has a negative 'repeat' field (%d)",
                   error_prefix.c_str(), profile_name.c_str(), json_desc["repeat"].GetInt());
            repeat = static_cast<unsigned int>(json_desc["repeat"].GetInt());
        }
        data->repeat = repeat;

        xbt_assert(data->repeat > 0, "%s: profile '%s' has a non-strictly-positive 'repeat' field (%d)",
                   error_prefix.c_str(), profile_name.c_str(), data->repeat);

        xbt_assert(json_desc.HasMember("seq"), "%s: profile '%s' has no 'seq' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["seq"].IsArray(), "%s: profile '%s' has a non-array 'seq' field",
                   error_prefix.c_str(), profile_name.c_str());
        const Value & seq = json_desc["seq"];
        xbt_assert(seq.Size() > 0, "%s: profile '%s' has an invalid array 'seq': its size must be "
                   "strictly positive", error_prefix.c_str(), profile_name.c_str());
        data->sequence.reserve(seq.Size());
        for (unsigned int i = 0; i < seq.Size(); ++i)
        {
            data->sequence.push_back(string(seq[i].GetString()));
        }

        profile->data = data;
    }
    else if (profile_type == "ptask_on_storage_homogeneous")
    {
        profile->type = ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS;
        ParallelTaskOnStorageHomogeneousProfileData * data = new ParallelTaskOnStorageHomogeneousProfileData;

        xbt_assert(json_desc.HasMember("bytes_to_read") or json_desc.HasMember("bytes_to_write"), "%s: profile '%s' has no 'bytes_to_read' or 'bytes_to_write' field (0 if not set)",
                   error_prefix.c_str(), profile_name.c_str());
        if (json_desc.HasMember("bytes_to_read"))
        {
            xbt_assert(json_desc["bytes_to_read"].IsNumber(), "%s: profile '%s' has a non-number 'bytes_to_read' field",
                       error_prefix.c_str(), profile_name.c_str());
            data->bytes_to_read = json_desc["bytes_to_read"].GetDouble();
        }
        if (json_desc.HasMember("bytes_to_write"))
        {
            xbt_assert(json_desc["bytes_to_write"].IsNumber(), "%s: profile '%s' has a non-number 'bytes_to_write' field",
                       error_prefix.c_str(), profile_name.c_str());
            data->bytes_to_write = json_desc["bytes_to_write"].GetDouble();
        }

        // If not set Use the "pfs" label by default
        if (json_desc.HasMember("storage") or json_desc.HasMember("host"))
        {
            string key;
            if (json_desc.HasMember("storage"))
            {
                key = "storage";
            }
            else
            {
                key = "host";

            }
            xbt_assert(json_desc[key.c_str()].IsString(),
                           "%s: profile '%s' has a non-string '%s' field",
                           error_prefix.c_str(), profile_name.c_str(),
                           key.c_str());
            data->storage_label = json_desc[key.c_str()].GetString();
        }

        // TODO: strategy
        profile->data = data;
    }
    else if (profile_type == "ptask_data_staging_between_storages")
    {
        profile->type = ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES;
        DataStagingProfileData * data = new DataStagingProfileData;

        xbt_assert(json_desc.HasMember("nb_bytes"), "%s: profile '%s' has no 'nb_bytes' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["nb_bytes"].IsNumber(), "%s: profile '%s' has a non-number 'nb_bytes' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->nb_bytes = json_desc["nb_bytes"].GetDouble();
        xbt_assert(data->nb_bytes >= 0, "%s: profile '%s' has a non-positive 'nb_bytes' field (%g)",
                   error_prefix.c_str(), profile_name.c_str(), data->nb_bytes);

        xbt_assert(json_desc.HasMember("from"), "%s: profile '%s' has no 'from' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["from"].IsString(),
                   "%s: profile '%s' has a non-string 'from' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->from_storage_label = json_desc["from"].GetString();

        xbt_assert(json_desc.HasMember("to"), "%s: profile '%s' has no 'to' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["to"].IsString(),
                   "%s: profile '%s' has a non-string 'to' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->to_storage_label = json_desc["to"].GetString();

        profile->data = data;

        // TODO: check that associated jobs request 0 resources
    }
    else if (profile_type == "send")
    {
        profile->type = ProfileType::SCHEDULER_SEND;
        SchedulerSendProfileData * data = new SchedulerSendProfileData;

        xbt_assert(json_desc.HasMember("msg"), "%s: profile '%s' has no 'msg' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["msg"].IsObject(), "%s: profile '%s' field 'msg' is no object",
                   error_prefix.c_str(), profile_name.c_str());

        data->message.CopyFrom(json_desc["msg"], data->message.GetAllocator());

        if (json_desc.HasMember("sleeptime"))
        {
            xbt_assert(json_desc["sleeptime"].IsNumber(),
                       "%s: profile '%s' has a non-number 'sleeptime' field",
                       error_prefix.c_str(), profile_name.c_str());
            data->sleeptime = json_desc["sleeptime"].GetDouble();
            xbt_assert(data->sleeptime > 0,
                       "%s: profile '%s' has a non-positive 'sleeptime' field (%g)",
                       error_prefix.c_str(), profile_name.c_str(), data->sleeptime);
        }
        else
        {
            data->sleeptime = 0.0000001;
        }
        profile->data = data;
    }
    else if (profile_type == "recv")
    {
        profile->type = ProfileType::SCHEDULER_RECV;
        SchedulerRecvProfileData * data = new SchedulerRecvProfileData;

        data->regex = string(".*");
        if (json_desc.HasMember("regex"))
        {
            data->regex = json_desc["regex"].GetString();
        }

        data->on_success = string("");
        if (json_desc.HasMember("success"))
        {
            data->on_success = json_desc["success"].GetString();
        }

        data->on_failure = string("");
        if (json_desc.HasMember("failure"))
        {
            data->on_failure = json_desc["failure"].GetString();
        }

        data->on_timeout = string("");
        if (json_desc.HasMember("timeout"))
        {
            data->on_timeout = json_desc["timeout"].GetString();
        }

        if (json_desc.HasMember("polltime"))
        {
            xbt_assert(json_desc["polltime"].IsNumber(),
                       "%s: profile '%s' has a non-number 'polltime' field",
                       error_prefix.c_str(), profile_name.c_str());
            data->polltime = json_desc["polltime"].GetDouble();
            xbt_assert(data->polltime > 0,
                       "%s: profile '%s' has a non-positive 'polltime' field (%g)",
                       error_prefix.c_str(), profile_name.c_str(), data->polltime);
        }
        else
        {
            data->polltime = 0.005;
        }
        profile->data = data;
    }
    else if (profile_type == "trace_replay")
    {
        xbt_assert(json_desc.HasMember("trace_type"), "%s: profile '%s' has no 'trace_type' field", error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["trace_type"].IsString(), "%s: profile '%s' has a non-string 'trace_type' field", error_prefix.c_str(), profile_name.c_str());
        const string trace_type = json_desc["trace_type"].GetString();

        // retrieve trace filenames
        xbt_assert(json_desc.HasMember("trace_file"), "%s: profile '%s' has no 'trace_file' field", error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["trace_file"].IsString(), "%s: profile '%s' has a non-string 'trace_file' field", error_prefix.c_str(), profile_name.c_str());
        const string trace_filename = json_desc["trace_file"].GetString();

        xbt_assert(is_from_a_file, "Trying to create a trace_replay profile from another source than a file workload, which is not implemented at the moment.");
        (void) is_from_a_file; // Avoids a warning if assertions are ignored

        fs::path base_dir = json_filename;
        base_dir = base_dir.parent_path();
        XBT_INFO("base_dir = '%s'", base_dir.string().c_str());
        xbt_assert(fs::exists(base_dir) && fs::is_directory(base_dir), "directory '%s' does not exist or is not a directory", base_dir.string().c_str());

        fs::path trace_path = base_dir.string() + "/" + trace_filename;
        xbt_assert(fs::exists(trace_path) && fs::is_regular_file(trace_path),
                   "Invalid JSON: profile '%s' has an invalid 'trace_file' field ('%s'), which leads to a non-existent file ('%s')",
                   profile_name.c_str(), trace_filename.c_str(), trace_path.string().c_str());

        ifstream trace_file(trace_path.string());
        xbt_assert(trace_file.is_open(), "Cannot open file '%s'", trace_path.string().c_str());

        std::vector<std::string> trace_filenames;
        string line;
        while (std::getline(trace_file, line))
        {
            boost::trim_right(line);
            fs::path rank_trace_path = trace_path.parent_path().string() + "/" + line;
            trace_filenames.push_back(rank_trace_path.string());
        }

        string filenames = boost::algorithm::join(trace_filenames, ", ");
        XBT_INFO("Filenames of profile '%s': [%s]", profile_name.c_str(), filenames.c_str());

        if (trace_type == "smpi")
        {
            profile->type = ProfileType::REPLAY_SMPI;
            auto * data = new ReplaySmpiProfileData;
            data->trace_filenames = trace_filenames;
            profile->data = data;
        }
        else if (trace_type == "usage")
        {
            profile->type = ProfileType::REPLAY_USAGE;
            auto * data = new ReplayUsageProfileData;
            data->trace_filenames = trace_filenames;
            profile->data = data;
        }
        else
        {
            xbt_die("Cannot create the profile '%s' of unknown trace type '%s'", profile_name.c_str(), trace_type.c_str());
        }
    }
    else
    {
        xbt_die("Cannot create the profile '%s' of unknown type '%s'",
                profile_name.c_str(), profile_type.c_str());
    }

    return profile;
}

// Do NOT remove namespaces in the arguments (to avoid doxygen warnings)
ProfilePtr Profile::from_json(const std::string & profile_name,
                            const std::string & json_str,
                            const std::string & error_prefix)
{
    Document doc;
    doc.Parse(json_str.c_str());
    xbt_assert(!doc.HasParseError(), "%s: Cannot be parsed. Content (between '##'):\n#%s#",
               error_prefix.c_str(), json_str.c_str());

    return Profile::from_json(profile_name, doc, error_prefix, false);
}

bool Profile::is_parallel_task() const
{
    return (type == ProfileType::PTASK) ||
           (type == ProfileType::PTASK_HOMOGENEOUS) ||
           (type == ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS) ||
           (type == ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES);
}

bool Profile::is_rigid() const
{
    return (type == ProfileType::PTASK) ||
           (type == ProfileType::REPLAY_SMPI) ||
           (type == ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES); // always uses 2 storages (and 0 compute nodes)
}

std::string profile_type_to_string(const ProfileType & type)
{
    string str;

    switch(type)
    {
    case ProfileType::DELAY:
        str = "DELAY";
        break;
    case ProfileType::PTASK:
        str = "PTASK";
        break;
    case ProfileType::PTASK_HOMOGENEOUS:
        str = "PTASK_HOMOGENEOUS";
        break;
    case ProfileType::REPLAY_SMPI:
        str = "REPLAY_SMPI";
        break;
    case ProfileType::REPLAY_USAGE:
        str = "REPLAY_USAGE";
        break;
    case ProfileType::SEQUENTIAL_COMPOSITION:
        str = "SEQUENTIAL_COMPOSITION";
        break;
    case ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS:
        str = "PTASK_ON_STORAGE_HOMOGENEOUS";
        break;
    case ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES:
        str = "PTASK_DATA_STAGING_BETWEEN_STORAGES";
        break;
    case ProfileType::SCHEDULER_SEND:
        str = "SCHEDULER_SEND";
        break;
    case ProfileType::SCHEDULER_RECV:
        str = "SCHEDULER_RECV";
        break;
    default:
        str = "unset";
        break;
    }

    return str;
}
