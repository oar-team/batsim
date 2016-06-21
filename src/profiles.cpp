/**
 * @file profiles.cpp
 * @brief Contains profile-related structures and classes
 */

#include "profiles.hpp"

#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <simgrid/msg.h>
#include <rapidjson/document.h>

using namespace std;
using namespace rapidjson;
using namespace boost;

XBT_LOG_NEW_DEFAULT_CATEGORY(profiles, "profiles"); //!< Logging

Profiles::Profiles()
{

}

Profiles::~Profiles()
{
    for (auto mit : _profiles)
    {
        delete mit.second;
    }
}

void Profiles::load_from_json(const Document &doc, const string & filename)
{
    xbt_assert(doc.IsObject());
    xbt_assert(doc.HasMember("profiles"), "Invalid JSON file '%s': the 'profiles' object is missing", filename.c_str());
    const Value & profiles = doc["profiles"];
    xbt_assert(profiles.IsObject(), "Invalid JSON file '%s': the 'profiles' member is not an object", filename.c_str());

    for (Value::ConstMemberIterator it = profiles.MemberBegin(); it != profiles.MemberEnd(); ++it)
    {
        const Value & key = it->name;
        const Value & value = it->value;

        xbt_assert(key.IsString(), "Invalid JSON file '%s': all children of the 'profiles' object must have a string key", filename.c_str());
        xbt_assert(value.IsObject(), "Invalid JSON file '%s': profile '%s' value must be an object", filename.c_str(), key.GetString());
        xbt_assert(value.HasMember("type"), "Invalid JSON file '%s': profile '%s' has no 'type' field", filename.c_str(), key.GetString());
        xbt_assert(value["type"].IsString(), "Invalid JSON file '%s': profile '%s' has a non-string 'type' field", filename.c_str(), key.GetString());

        Profile * profile = new Profile;

        string profileType = value["type"].GetString();
        if (profileType == "delay")
        {
            profile->type = ProfileType::DELAY;
            DelayProfileData * data = new DelayProfileData;

            xbt_assert(value.HasMember("delay"), "Invalid JSON file '%s': profile '%s' has no 'delay' field", filename.c_str(), key.GetString());
            xbt_assert(value["delay"].IsNumber(), "Invalid JSON file '%s': profile '%s' has a non-number 'delay' field", filename.c_str(), key.GetString());
            data->delay = value["delay"].GetDouble();

            profile->data = data;
        }
        else if (profileType == "msg_par")
        {
            profile->type = ProfileType::MSG_PARALLEL;
            MsgParallelProfileData * data = new MsgParallelProfileData;

            xbt_assert(value.HasMember("cpu"), "Invalid JSON file '%s': profile '%s' has no 'cpu' field", filename.c_str(), key.GetString());
            const Value & cpu = value["cpu"];
            xbt_assert(cpu.IsArray(), "Invalid JSON file '%s': profile '%s' has a non-array 'cpu' field", filename.c_str(), key.GetString());
            data->nb_res = cpu.Size();
            xbt_assert(data->nb_res > 0, "Invalid JSON file '%s': profile '%s' has an invalid-sized array 'cpu' (size=%d): must be strictly positive", filename.c_str(), key.GetString(), (int)cpu.Size());
            xbt_assert((int)cpu.Size() == data->nb_res, "Invalid JSON file '%s' : profile '%s' is incoherent: cpu array has size %d whereas nb_res is %d", filename.c_str(), key.GetString(), cpu.Size(), data->nb_res);
            data->cpu = new double[data->nb_res];
            for (unsigned int i = 0; i < cpu.Size(); ++i)
            {
                xbt_assert(cpu[i].IsNumber(), "Invalid JSON file '%s': profile '%s' computation array is invalid: all elements must be numbers", filename.c_str(), key.GetString());
                data->cpu[i] = cpu[i].GetDouble();
            }

            xbt_assert(value.HasMember("com"), "Invalid JSON file '%s': profile '%s' has no 'com' field", filename.c_str(), key.GetString());
            const Value & com = value["com"];
            xbt_assert(com.IsArray(), "Invalid JSON file '%s': profile '%s' has a non-array 'com' field", filename.c_str(), key.GetString());
            xbt_assert((int)com.Size() == data->nb_res * data->nb_res, "Invalid JSON file '%s' : profile '%s' is incoherent: com array has size %d whereas nb_res is %d", filename.c_str(), key.GetString(), com.Size(), data->nb_res);
            data->com = new double[data->nb_res * data->nb_res];
            for (unsigned int i = 0; i < com.Size(); ++i)
            {
                xbt_assert(com[i].IsNumber(), "Invalid JSON file '%s': profile '%s' communication array is invalid: all elements must be numbers", filename.c_str(), key.GetString());
                data->com[i] = com[i].GetDouble();
                xbt_assert(data->com[i] >= 0, "Invalid JSON file '%s': profile '%s' communication array is invalid: all elements must be non-negative", filename.c_str(), key.GetString());
            }

            profile->data = data;
        }
        else if (profileType == "msg_par_hg")
        {
            profile->type = ProfileType::MSG_PARALLEL_HOMOGENEOUS;
            MsgParallelHomogeneousProfileData * data = new MsgParallelHomogeneousProfileData;

            xbt_assert(value.HasMember("cpu"), "Invalid JSON file '%s': profile '%s' has no 'cpu' field", filename.c_str(), key.GetString());
            xbt_assert(value["cpu"].IsNumber(), "Invalid JSON file '%s': profile '%s' has a non-number 'cpu' field", filename.c_str(), key.GetString());
            data->cpu = value["cpu"].GetDouble();

            xbt_assert(value.HasMember("com"), "Invalid JSON file '%s': profile '%s' has no 'com' field", filename.c_str(), key.GetString());
            xbt_assert(value["com"].IsNumber(), "Invalid JSON file '%s': profile '%s' has a non-number 'com' field", filename.c_str(), key.GetString());
            data->com = value["com"].GetDouble();

            profile->data = data;
        }
        else if (profileType == "composed")
        {
            profile->type = ProfileType::SEQUENCE;
            SequenceProfileData * data = new SequenceProfileData;

            xbt_assert(value.HasMember("nb"), "Invalid JSON file '%s': profile '%s' has no 'nb' field", filename.c_str(), key.GetString());
            xbt_assert(value["nb"].IsInt(), "Invalid JSON file '%s': profile '%s' has a non-integral 'nb' field", filename.c_str(), key.GetString());
            data->repeat = value["nb"].GetInt();
            xbt_assert(data->repeat > 0, "Invalid JSON file '%s': profile '%s' has a non-strictly-positive 'nb' field (%d)", filename.c_str(), key.GetString(), data->repeat);

            xbt_assert(value.HasMember("seq"), "Invalid JSON file '%s': profile '%s' has no 'seq' field", filename.c_str(), key.GetString());
            xbt_assert(value["seq"].IsArray(), "Invalid JSON file '%s': profile '%s' has a non-array 'seq' field", filename.c_str(), key.GetString());
            const Value & seq = value["seq"];
            xbt_assert(seq.Size() > 0, "Invalid JSON file '%s': profile '%s' has an invalid array 'seq': its size must be strictly positive", filename.c_str(), key.GetString());
            for (unsigned int i = 0; i < seq.Size(); ++i)

                profile->data = data;
        }
        else if (profileType == "smpi")
        {
            profile->type = ProfileType::SMPI;
            SmpiProfileData * data = new SmpiProfileData;

            xbt_assert(value.HasMember("trace"), "Invalid JSON file '%s': profile '%s' has no 'trace' field", filename.c_str(), key.GetString());
            xbt_assert(value["trace"].IsString(), "Invalid JSON file '%s': profile '%s' has a non-string 'trace' field", filename.c_str(), key.GetString());
            const string trace = value["trace"].GetString();

            filesystem::path baseDir(filename);
            baseDir = baseDir.parent_path();
            xbt_assert(filesystem::exists(baseDir) && filesystem::is_directory(baseDir));

            //XBT_DEBUG("baseDir = '%s'", baseDir.string().c_str());
            //XBT_DEBUG("trace = '%s'", trace.c_str());

            filesystem::path tracePath(baseDir.string() + "/" + trace);
            xbt_assert(filesystem::exists(tracePath) && filesystem::is_regular_file(tracePath),
                       "Invalid JSON file '%s': profile '%s' has an invalid 'trace' field ('%s'), which lead to a non-existent file ('%s')",
                       filename.c_str(), key.GetString(), trace.c_str(), string(filename + "/" + trace).c_str());

            ifstream traceFile(tracePath.string());
            xbt_assert(traceFile.is_open(), "Cannot open file '%s'", tracePath.string().c_str());

            string line;
            while (std::getline(traceFile, line))
            {
                trim_right(line);
                filesystem::path rank_trace_path(tracePath.parent_path().string() + "/" + line);
                data->trace_filenames.push_back("./" + rank_trace_path.string());
            }

            string filenames = boost::algorithm::join(data->trace_filenames, ", ");
            XBT_DEBUG("Filenames of profile '%s': [%s]", key.GetString(), filenames.c_str());

            profile->data = data;
        }

        xbt_assert(!exists(string(key.GetString())), "Invalid JSON file '%s': duplication of profile name '%s'", filename.c_str(), key.GetString());
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

bool Profiles::exists(const std::string &profile_name) const
{
    auto mit = _profiles.find(profile_name);
    return mit != _profiles.end();
}

const std::map<string, Profile *> Profiles::profiles() const
{
    return _profiles;
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
    else
        XBT_ERROR("Deletion of an unknown profile type (%d)", type);
}
