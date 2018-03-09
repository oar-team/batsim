/**
 * @file profiles.cpp
 * @brief Contains profile-related structures and classes
 */

#include "profiles.hpp"

#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <simgrid/msg.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

using namespace std;
using namespace rapidjson;
using namespace boost;

XBT_LOG_NEW_DEFAULT_CATEGORY(profiles, "profiles"); //!< Logging

Profiles::~Profiles()
{
    for (auto mit : _profiles)
    {
        delete mit.second;
    }
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

        Profile * profile = Profile::from_json(profile_name, value, error_prefix,
                                               true, filename);

        xbt_assert(!exists(string(key.GetString())), "%s: duplication of profile name '%s'",
                   error_prefix.c_str(), key.GetString());
        _profiles[string(key.GetString())] = profile;
    }
}

Profile *Profiles::operator[](const std::string &profile_name)
{
    auto mit = _profiles.find(profile_name);
    xbt_assert(mit != _profiles.end(), "Cannot get profile '%s': it does not exist", profile_name.c_str());
    return mit->second;
}

const Profile *Profiles::operator[](const std::string &profile_name) const
{
    auto mit = _profiles.find(profile_name);
    xbt_assert(mit != _profiles.end(), "Cannot get profile '%s': it does not exist", profile_name.c_str());
    return mit->second;
}

Profile * Profiles::at(const std::string & profile_name)
{
    return operator[](profile_name);
}

const Profile * Profiles::at(const std::string & profile_name) const
{
    return operator[](profile_name);
}

bool Profiles::exists(const std::string &profile_name) const
{
    auto mit = _profiles.find(profile_name);
    return mit != _profiles.end();
}

void Profiles::add_profile(const std::string & profile_name,
                           Profile *profile)
{
    xbt_assert(!exists(profile_name),
               "Bad Profiles::add_profile call: A profile with name='%s' already exists.",
               profile_name.c_str());

    _profiles[profile_name] = profile;
}

const std::map<std::string, Profile *> Profiles::profiles() const
{
    return _profiles;
}

int Profiles::nb_profiles() const
{
    return _profiles.size();
}


MsgParallelProfileData::~MsgParallelProfileData()
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
    if (type == ProfileType::DELAY)
    {
        DelayProfileData * d = (DelayProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::MSG_PARALLEL)
    {
        MsgParallelProfileData * d = (MsgParallelProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::MSG_PARALLEL_HOMOGENEOUS)
    {
        MsgParallelHomogeneousProfileData * d = (MsgParallelHomogeneousProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::MSG_PARALLEL_HOMOGENEOUS_TOTAL_AMOUNT)
    {
        MsgParallelHomogeneousTotalAmountProfileData * d = (MsgParallelHomogeneousTotalAmountProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::SMPI)
    {
        SmpiProfileData * d = (SmpiProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::SEQUENCE)
    {
        SequenceProfileData * d = (SequenceProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS)
    {
        MsgParallelHomogeneousPFSMultipleTiersProfileData * d = (MsgParallelHomogeneousPFSMultipleTiersProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::MSG_DATA_STAGING)
    {
        MsgDataStagingProfileData * d = (MsgDataStagingProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::SCHEDULER_SEND)
    {
        SchedulerSendProfileData * d = (SchedulerSendProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else if (type == ProfileType::SCHEDULER_RECV)
    {
        SchedulerRecvProfileData * d = (SchedulerRecvProfileData *) data;
        if (d != nullptr)
        {
            delete d;
            d = nullptr;
        }
    }
    else
    {
        XBT_ERROR("Deletion of an unknown profile type (%d)", type);
    }
}

// Do NOT remove namespaces in the arguments (to avoid doxygen warnings)
Profile *Profile::from_json(const std::string & profile_name,
                            const rapidjson::Value & json_desc,
                            const std::string & error_prefix,
                            bool is_from_a_file,
                            const std::string & json_filename)
{
    (void) error_prefix; // Avoids a warning if assertions are ignored

    Profile * profile = new Profile;
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
    else if (profile_type == "msg_par")
    {
        profile->type = ProfileType::MSG_PARALLEL;
        MsgParallelProfileData * data = new MsgParallelProfileData;

        xbt_assert(json_desc.HasMember("cpu"), "%s: profile '%s' has no 'cpu' field",
                   error_prefix.c_str(), profile_name.c_str());
        const Value & cpu = json_desc["cpu"];
        xbt_assert(cpu.IsArray(), "%s: profile '%s' has a non-array 'cpu' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->nb_res = cpu.Size();
        xbt_assert(data->nb_res > 0, "%s: profile '%s' has an invalid-sized array 'cpu' (size=%d): "
                   "must be strictly positive",
                   error_prefix.c_str(), profile_name.c_str(), (int) cpu.Size());
        xbt_assert((int)cpu.Size() == data->nb_res, "%s: profile '%s' is incoherent: cpu array has "
                   "size %d whereas nb_res is %d",
                   error_prefix.c_str(), profile_name.c_str(), cpu.Size(), data->nb_res);
        data->cpu = new double[data->nb_res];
        for (unsigned int i = 0; i < cpu.Size(); ++i)
        {
            xbt_assert(cpu[i].IsNumber(), "%s: profile '%s' computation array is invalid: all "
                       "elements must be numbers", error_prefix.c_str(), profile_name.c_str());
            data->cpu[i] = cpu[i].GetDouble();
            xbt_assert(data->cpu[i] >= 0, "%s: profile '%s' computation array is invalid: all "
                       "elements must be non-negative", error_prefix.c_str(), profile_name.c_str());
        }

        xbt_assert(json_desc.HasMember("com"), "%s: profile '%s' has no 'com' field",
                   error_prefix.c_str(), profile_name.c_str());
        const Value & com = json_desc["com"];
        xbt_assert(com.IsArray(), "%s: profile '%s' has a non-array 'com' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert((int)com.Size() == data->nb_res * data->nb_res, "%s: profile '%s' is incoherent:"
                   "com array has size %d whereas nb_res is %d",
                   error_prefix.c_str(), profile_name.c_str(), com.Size(), data->nb_res);
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
    else if (profile_type == "msg_par_hg")
    {
        profile->type = ProfileType::MSG_PARALLEL_HOMOGENEOUS;
        MsgParallelHomogeneousProfileData * data = new MsgParallelHomogeneousProfileData;

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

        profile->data = data;
    }
    else if (profile_type == "msg_par_hg_tot")
    {
        profile->type = ProfileType::MSG_PARALLEL_HOMOGENEOUS_TOTAL_AMOUNT;
        MsgParallelHomogeneousTotalAmountProfileData * data = new MsgParallelHomogeneousTotalAmountProfileData;

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

        profile->data = data;
    }
    else if (profile_type == "composed")
    {
        profile->type = ProfileType::SEQUENCE;
        SequenceProfileData * data = new SequenceProfileData;

        int repeat = 1;
        if (json_desc.HasMember("nb"))
        {
            xbt_assert(json_desc["nb"].IsInt(), "%s: profile '%s' has a non-integral 'nb' field",
                   error_prefix.c_str(), profile_name.c_str());
            repeat = json_desc["nb"].GetInt();
        }
        data->repeat = repeat;

        xbt_assert(data->repeat > 0, "%s: profile '%s' has a non-strictly-positive 'nb' field (%d)",
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
    else if (profile_type == "msg_par_hg_pfs_tiers" || profile_type == "msg_par_hg_pfs0")
    {
        /*
        {
            "size": 10e5,
            "direction": "to_storage",
            "storage": "pfs"
        }
        */
        profile->type = ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS;
        MsgParallelHomogeneousPFSMultipleTiersProfileData * data = new MsgParallelHomogeneousPFSMultipleTiersProfileData;

        xbt_assert(json_desc.HasMember("size"), "%s: profile '%s' has no 'size' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["size"].IsNumber(), "%s: profile '%s' has a non-number 'size' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->size = json_desc["size"].GetDouble();
        xbt_assert(data->size >= 0, "%s: profile '%s' has a non-positive 'size' field (%g)",
                   error_prefix.c_str(), profile_name.c_str(), data->size);

        if (json_desc.HasMember("direction"))
        {
            xbt_assert(json_desc["direction"].IsString(),
                       "%s: profile '%s' has a non-string 'direction' field",
                       error_prefix.c_str(), profile_name.c_str());
            string direction = json_desc["direction"].GetString();

            if (direction == "from_node_to_storage" || direction == "to_storage")
            {
                data->direction = MsgParallelHomogeneousPFSMultipleTiersProfileData::Direction::FROM_NODES_TO_STORAGE;
            }
            else if (direction == "from_storage_to_node" || direction == "from_storage")
            {
                data->direction = MsgParallelHomogeneousPFSMultipleTiersProfileData::Direction::FROM_STORAGE_TO_NODES;
            }
            else
            {
                xbt_assert(false, "%s: profile '%s' has an invalid 'direction' field (%s)",
                           error_prefix.c_str(), profile_name.c_str(), direction.c_str());
            }
        }
        else
        {
            data->direction = MsgParallelHomogeneousPFSMultipleTiersProfileData::Direction::FROM_NODES_TO_STORAGE;
        }

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

                xbt_assert(json_desc[key.c_str()].IsString(),
                               "%s: profile '%s' has a non-string '%s' field",
                               error_prefix.c_str(), profile_name.c_str(), key);
                data->storage_label = json_desc[key.c_str()].GetString();
            }
        }
        else
        {
            // Use the "pfs" label by default
            data->storage_label = "pfs";
        }

        profile->data = data;
    }
    else if (profile_type == "data_staging")
    {
        /*
        {
            "size": 10e5,
            "from": "pfs",
            "to": "lcfs"
        }
        */
        profile->type = ProfileType::MSG_DATA_STAGING;
        MsgDataStagingProfileData * data = new MsgDataStagingProfileData;

        xbt_assert(json_desc.HasMember("size"), "%s: profile '%s' has no 'size' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["size"].IsNumber(), "%s: profile '%s' has a non-number 'size' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->size = json_desc["size"].GetDouble();
        xbt_assert(data->size >= 0, "%s: profile '%s' has a non-positive 'size' field (%g)",
                   error_prefix.c_str(), profile_name.c_str(), data->size);

        xbt_assert(json_desc.HasMember("from"), "%s: profile '%s' has no 'from' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["from"].IsInt(),
                   "%s: profile '%s' has a non-string 'from' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->from_storage_label = json_desc["from"].GetInt();

        xbt_assert(json_desc.HasMember("to"), "%s: profile '%s' has no 'to' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["to"].IsInt(),
                   "%s: profile '%s' has a non-string 'to' field",
                   error_prefix.c_str(), profile_name.c_str());
        data->to_storage_label = json_desc["to"].GetInt();

        profile->data = data;
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
    else if (profile_type == "smpi")
    {
        profile->type = ProfileType::SMPI;
        SmpiProfileData * data = new SmpiProfileData;

        xbt_assert(json_desc.HasMember("trace"), "%s: profile '%s' has no 'trace' field",
                   error_prefix.c_str(), profile_name.c_str());
        xbt_assert(json_desc["trace"].IsString(), "%s: profile '%s' has a non-string 'trace' field",
                   error_prefix.c_str(), profile_name.c_str());
        const string trace_filename = json_desc["trace"].GetString();

        xbt_assert(is_from_a_file, "Trying to create a SMPI profile from another source than "
                   "a file workload, which is not implemented at the moment.");
        (void) is_from_a_file; // Avoids a warning if assertions are ignored

        filesystem::path base_dir(json_filename);
        base_dir = base_dir.parent_path();
        XBT_INFO("base_dir = '%s'", base_dir.string().c_str());
        xbt_assert(filesystem::exists(base_dir) && filesystem::is_directory(base_dir));

        //XBT_INFO("base_dir = '%s'", base_dir.string().c_str());
        //XBT_INFO("trace = '%s'", trace.c_str());
        filesystem::path trace_path(base_dir.string() + "/" + trace_filename);
        //XBT_INFO("trace_path = '%s'", trace_path.string().c_str());
        xbt_assert(filesystem::exists(trace_path) && filesystem::is_regular_file(trace_path),
                   "Invalid JSON: profile '%s' has an invalid 'trace' field ('%s'), which leads to a non-existent file ('%s')",
                   profile_name.c_str(), trace_filename.c_str(), trace_path.string().c_str());

        ifstream trace_file(trace_path.string());
        xbt_assert(trace_file.is_open(), "Cannot open file '%s'", trace_path.string().c_str());

        string line;
        while (std::getline(trace_file, line))
        {
            trim_right(line);
            filesystem::path rank_trace_path(trace_path.parent_path().string() + "/" + line);
            data->trace_filenames.push_back(rank_trace_path.string());
        }

        string filenames = boost::algorithm::join(data->trace_filenames, ", ");
        XBT_INFO("Filenames of profile '%s': [%s]", profile_name.c_str(), filenames.c_str());

        profile->data = data;
    }

    // Let's get the JSON string which describes the profile (to conserve potential fields unused by Batsim)
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_desc.Accept(writer);
    profile->json_description = string(buffer.GetString(), buffer.GetSize());

    return profile;
}

// Do NOT remove namespaces in the arguments (to avoid doxygen warnings)
Profile *Profile::from_json(const std::string & profile_name,
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
    return (type == ProfileType::MSG_PARALLEL) ||
           (type == ProfileType::MSG_PARALLEL_HOMOGENEOUS) ||
           (type == ProfileType::MSG_PARALLEL_HOMOGENEOUS_TOTAL_AMOUNT) ||
           (type == ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS) ||
           (type == ProfileType::MSG_DATA_STAGING);
}


std::string profile_type_to_string(const ProfileType & type)
{
    string str = "unset";

    switch(type)
    {
    case ProfileType::DELAY:
        str = "DELAY";
        break;
    case ProfileType::MSG_PARALLEL:
        str = "MSG_PARALLEL";
        break;
    case ProfileType::MSG_PARALLEL_HOMOGENEOUS:
        str = "MSG_PARALLEL_HOMOGENEOUS";
        break;
    case ProfileType::MSG_PARALLEL_HOMOGENEOUS_TOTAL_AMOUNT:
        str = "MSG_PARALLEL_HOMOGENEOUS_TOTAL_AMOUNT";
        break;
    case ProfileType::SMPI:
        str = "SMPI";
        break;
    case ProfileType::SEQUENCE:
        str = "SEQUENCE";
        break;
    case ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS:
        str = "MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS";
        break;
    case ProfileType::MSG_DATA_STAGING:
        str = "MSG_DATA_STAGING";
        break;
    case ProfileType::SCHEDULER_SEND:
        str = "SCHEDULER_SEND";
        break;
    case ProfileType::SCHEDULER_RECV:
        str = "SCHEDULER_RECV";
        break;
    }

    return str;
}
