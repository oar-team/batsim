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

        xbt_assert(key.IsString(),
                   "Invalid JSON file '%s': all children of the 'profiles' object must have a string key",
                   filename.c_str());
        string profile_name = key.GetString();

        Profile * profile = Profile::from_json(profile_name, value);

        xbt_assert(!exists(string(key.GetString())),
                   "Invalid JSON file '%s': duplication of profile name '%s'",
                   filename.c_str(), key.GetString());
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

Profile *Profile::from_json(const std::string &profile_name, const rapidjson::Value &json_desc)
{
    Profile * profile = new Profile;

    xbt_assert(json_desc.IsObject(), "Invalid JSON: profile '%s' value must be an object", profile_name.c_str());
    xbt_assert(json_desc.HasMember("type"), "Invalid JSON: profile '%s' has no 'type' field", profile_name.c_str());
    xbt_assert(json_desc["type"].IsString(), "Invalid JSON: profile '%s' has a non-string 'type' field", profile_name.c_str());

    string profile_type = json_desc["type"].GetString();
    if (profile_type == "delay")
    {
        profile->type = ProfileType::DELAY;
        DelayProfileData * data = new DelayProfileData;

        xbt_assert(json_desc.HasMember("delay"), "Invalid JSON: profile '%s' has no 'delay' field", profile_name.c_str());
        xbt_assert(json_desc["delay"].IsNumber(), "Invalid JSON: profile '%s' has a non-number 'delay' field", profile_name.c_str());
        data->delay = json_desc["delay"].GetDouble();

        xbt_assert(data->delay > 0, "Invalid JSON: profile '%s' has a non-strictly-positive 'delay' field (%g)",
                   profile_name.c_str(), data->delay);

        profile->data = data;
    }
    else if (profile_type == "msg_par")
    {
        profile->type = ProfileType::MSG_PARALLEL;
        MsgParallelProfileData * data = new MsgParallelProfileData;

        xbt_assert(json_desc.HasMember("cpu"), "Invalid JSON: profile '%s' has no 'cpu' field", profile_name.c_str());
        const Value & cpu = json_desc["cpu"];
        xbt_assert(cpu.IsArray(), "Invalid JSON: profile '%s' has a non-array 'cpu' field", profile_name.c_str());
        data->nb_res = cpu.Size();
        xbt_assert(data->nb_res > 0, "Invalid JSON: profile '%s' has an invalid-sized array 'cpu' (size=%d): must be strictly positive", profile_name.c_str(), (int)cpu.Size());
        xbt_assert((int)cpu.Size() == data->nb_res, "Invalid JSON : profile '%s' is incoherent: cpu array has size %d whereas nb_res is %d", profile_name.c_str(), cpu.Size(), data->nb_res);
        data->cpu = new double[data->nb_res];
        for (unsigned int i = 0; i < cpu.Size(); ++i)
        {
            xbt_assert(cpu[i].IsNumber(), "Invalid JSON: profile '%s' computation array is invalid: all elements must be numbers", profile_name.c_str());
            data->cpu[i] = cpu[i].GetDouble();
            xbt_assert(data->cpu[i] >= 0, "Invalid JSON: profile '%s' computation array is invalid: all elements must be non-negative", profile_name.c_str());
        }

        xbt_assert(json_desc.HasMember("com"), "Invalid JSON: profile '%s' has no 'com' field", profile_name.c_str());
        const Value & com = json_desc["com"];
        xbt_assert(com.IsArray(), "Invalid JSON: profile '%s' has a non-array 'com' field", profile_name.c_str());
        xbt_assert((int)com.Size() == data->nb_res * data->nb_res, "Invalid JSON : profile '%s' is incoherent: com array has size %d whereas nb_res is %d", profile_name.c_str(), com.Size(), data->nb_res);
        data->com = new double[data->nb_res * data->nb_res];
        for (unsigned int i = 0; i < com.Size(); ++i)
        {
            xbt_assert(com[i].IsNumber(), "Invalid JSON: profile '%s' communication array is invalid: all elements must be numbers", profile_name.c_str());
            data->com[i] = com[i].GetDouble();
            xbt_assert(data->com[i] >= 0, "Invalid JSON: profile '%s' communication array is invalid: all elements must be non-negative", profile_name.c_str());
        }

        profile->data = data;
    }
    else if (profile_type == "msg_par_hg")
    {
        profile->type = ProfileType::MSG_PARALLEL_HOMOGENEOUS;
        MsgParallelHomogeneousProfileData * data = new MsgParallelHomogeneousProfileData;

        xbt_assert(json_desc.HasMember("cpu"), "Invalid JSON: profile '%s' has no 'cpu' field", profile_name.c_str());
        xbt_assert(json_desc["cpu"].IsNumber(), "Invalid JSON: profile '%s' has a non-number 'cpu' field", profile_name.c_str());
        data->cpu = json_desc["cpu"].GetDouble();
        xbt_assert(data->cpu >= 0, "Invalid JSON: profile '%s' has a non-positive 'cpu' field (%g)", profile_name.c_str(), data->cpu);

        xbt_assert(json_desc.HasMember("com"), "Invalid JSON: profile '%s' has no 'com' field", profile_name.c_str());
        xbt_assert(json_desc["com"].IsNumber(), "Invalid JSON: profile '%s' has a non-number 'com' field", profile_name.c_str());
        data->com = json_desc["com"].GetDouble();
        xbt_assert(data->com >= 0, "Invalid JSON: profile '%s' has a non-positive 'com' field (%g)", profile_name.c_str(), data->com);

        profile->data = data;
    }
    else if (profile_type == "composed")
    {
        profile->type = ProfileType::SEQUENCE;
        SequenceProfileData * data = new SequenceProfileData;

        xbt_assert(json_desc.HasMember("nb"), "Invalid JSON: profile '%s' has no 'nb' field", profile_name.c_str());
        xbt_assert(json_desc["nb"].IsInt(), "Invalid JSON: profile '%s' has a non-integral 'nb' field", profile_name.c_str());
        data->repeat = json_desc["nb"].GetInt();
        xbt_assert(data->repeat > 0, "Invalid JSON: profile '%s' has a non-strictly-positive 'nb' field (%d)", profile_name.c_str(), data->repeat);

        xbt_assert(json_desc.HasMember("seq"), "Invalid JSON: profile '%s' has no 'seq' field", profile_name.c_str());
        xbt_assert(json_desc["seq"].IsArray(), "Invalid JSON: profile '%s' has a non-array 'seq' field", profile_name.c_str());
        const Value & seq = json_desc["seq"];
        xbt_assert(seq.Size() > 0, "Invalid JSON: profile '%s' has an invalid array 'seq': its size must be strictly positive", profile_name.c_str());
        for (unsigned int i = 0; i < seq.Size(); ++i)

            profile->data = data;
    }
    else if (profile_type == "smpi")
    {
        xbt_assert(false, "Oops, SMPI jobs have been disabled (when dynamic jobs have been added). "
                   "Please update source code to make them work with static & dynamic jobs");

        /*profile->type = ProfileType::SMPI;
        SmpiProfileData * data = new SmpiProfileData;

        xbt_assert(json_desc.HasMember("trace"), "Invalid JSON: profile '%s' has no 'trace' field", profile_name.c_str());
        xbt_assert(json_desc["trace"].IsString(), "Invalid JSON: profile '%s' has a non-string 'trace' field", profile_name.c_str());
        const string trace = json_desc["trace"].GetString();*/

        /*filesystem::path baseDir(filename);
        baseDir = baseDir.parent_path();
        xbt_assert(filesystem::exists(baseDir) && filesystem::is_directory(baseDir));*/

        //XBT_DEBUG("baseDir = '%s'", baseDir.string().c_str());
        //XBT_DEBUG("trace = '%s'", trace.c_str());

        /*filesystem::path tracePath(baseDir.string() + "/" + trace);
        xbt_assert(filesystem::exists(tracePath) && filesystem::is_regular_file(tracePath),
                   "Invalid JSON: profile '%s' has an invalid 'trace' field ('%s'), which lead to a non-existent file ('%s')",
                   filename.c_str(), profile_name.c_str(), trace.c_str(), string(filename + "/" + trace).c_str());

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
        XBT_DEBUG("Filenames of profile '%s': [%s]", profile_name.c_str(), filenames.c_str());

        profile->data = data;*/
    }

    // Let's get the JSON string which describes the profile (to conserve potential fields unused by Batsim)
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_desc.Accept(writer);
    profile->json_description = buffer.GetString();

    return profile;
}

Profile *Profile::from_json(const std::string & profile_name, const std::string & json_str)
{
    Document doc;
    doc.Parse(json_str.c_str());

    return Profile::from_json(profile_name, doc);
}
