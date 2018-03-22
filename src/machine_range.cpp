/**
 * @file machine_range.cpp
 * @brief Contains the class which handles set of machines
 */

#include "machine_range.hpp"

#include <vector>

#include <boost/algorithm/string.hpp>

#include <simgrid/msg.h>


XBT_LOG_NEW_DEFAULT_CATEGORY(machine_range, "machine_range"); //!< Logging

using namespace std;

MachineRange::Set::element_iterator MachineRange::elements_begin()
{
    return boost::icl::elements_begin(set);
}

MachineRange::Set::element_const_iterator MachineRange::elements_begin() const
{
    return boost::icl::elements_begin(set);
}

MachineRange::Set::element_iterator MachineRange::elements_end()
{
    return boost::icl::elements_end(set);
}

MachineRange::Set::element_const_iterator MachineRange::elements_end() const
{
    return boost::icl::elements_end(set);
}

MachineRange::Set::iterator MachineRange::intervals_begin()
{
    return set.begin();
}

MachineRange::Set::const_iterator MachineRange::intervals_begin() const
{
    return set.begin();
}

MachineRange::Set::iterator MachineRange::intervals_end()
{
    return set.end();
}

MachineRange::Set::const_iterator MachineRange::intervals_end() const
{
    return set.end();
}


void MachineRange::clear()
{
    set.clear();
    xbt_assert(size() == 0);
}

void MachineRange::insert(const MachineRange &range)
{
    for (auto it = range.intervals_begin(); it != range.intervals_end(); ++it)
    {
        set.insert(*it);
    }
}

void MachineRange::insert(ClosedInterval interval)
{
    set.insert(interval);
}

void MachineRange::insert(int value)
{
    set.insert(value);
}

void MachineRange::remove(const MachineRange &range)
{
    set -= range.set;
}

void MachineRange::remove(ClosedInterval interval)
{
    set -= interval;
}

void MachineRange::remove(int value)
{
    set -= value;
}

int MachineRange::first_element() const
{
    xbt_assert(size() > 0);
    return *elements_begin();
}

unsigned int MachineRange::size() const
{
    return set.size();
}

bool MachineRange::contains(int machine_id) const
{
    return boost::icl::contains(set, machine_id);
}

std::string MachineRange::to_string_brackets(const std::string & union_str,
                                             const std::string & opening_bracket,
                                             const std::string & closing_bracket,
                                             const std::string & sep) const
{
    vector<string> machine_id_strings;
    for (auto it = intervals_begin(); it != intervals_end(); ++it)
    {
        if (it->lower() == it->upper())
        {
            machine_id_strings.push_back(opening_bracket + to_string(it->lower()) + closing_bracket);
        }
        else
        {
            machine_id_strings.push_back(opening_bracket + to_string(it->lower()) + sep + to_string(it->upper()) + closing_bracket);
        }
    }

    return boost::algorithm::join(machine_id_strings, union_str);
}

std::string MachineRange::to_string_hyphen(const std::string &sep, const std::string &joiner) const
{
    vector<string> machine_id_strings;
    for (auto it = intervals_begin(); it != intervals_end(); ++it)
    {
        if (it->lower() == it->upper())
        {
            machine_id_strings.push_back(to_string(it->lower()));
        }
        else
        {
            machine_id_strings.push_back(to_string(it->lower()) + joiner + to_string(it->upper()));
        }
    }

    return boost::algorithm::join(machine_id_strings, sep);
}

string MachineRange::to_string_elements(const string &sep) const
{
    vector<string> machine_id_strings;
    for (auto it = elements_begin(); it != elements_end(); ++it)
    {
        machine_id_strings.push_back(to_string(*it));
    }

    return boost::algorithm::join(machine_id_strings, sep);
}

MachineRange MachineRange::from_string_hyphen(const string & str,
                                              const string & sep,
                                              const string & joiner,
                                              const string & error_prefix)
{
    (void) error_prefix; // Avoids a warning if assertions are ignored
    MachineRange res;

    // Let us do a split by sep to get all the parts
    vector<string> parts;
    boost::split(parts, str, boost::is_any_of(sep), boost::token_compress_on);

    for (const string & part : parts)
    {
        // Since each machineIDk can either be a single machine or a closed interval, let's try to split on joiner
        vector<string> interval_parts;
        boost::split(interval_parts, part, boost::is_any_of(joiner), boost::token_compress_on);
        xbt_assert(interval_parts.size() >= 1 && interval_parts.size() <= 2,
                   "%s: The part '%s' (from '%s') should either be a single machine ID "
                   "(syntax: MID to represent the machine ID) or a closed interval "
                   "(syntax: MIDa-MIDb to represent the machine interval [MIDA,MIDb])",
                   error_prefix.c_str(), part.c_str(), str.c_str());

        try
        {
            if (interval_parts.size() == 1)
            {
                int machine_id = std::stoi(interval_parts[0]);
                res.insert(machine_id);
            }
            else
            {
                int machineIDa = std::stoi(interval_parts[0]);
                int machineIDb = std::stoi(interval_parts[1]);

                xbt_assert(machineIDa <= machineIDb,
                           "%s: The part '%s' (from '%s') follows the MIDa-MIDb syntax. "
                           "However, the first value should be lesser than or equal to the second one",
                           error_prefix.c_str(), part.c_str(), str.c_str());

                res.insert(MachineRange::ClosedInterval(machineIDa, machineIDb));
            }
        }
        catch(const std::exception &)
        {
            XBT_CRITICAL("%s: Could not read integers from part '%s' (from '%s'). "
                         "The part should either be a single machine ID "
                         "(syntax: MID to represent the machine ID) or a closed interval "
                         "(syntax: MIDa-MIDb to represent the machine interval [MIDA,MIDb])",
                         error_prefix.c_str(), part.c_str(), str.c_str());
            xbt_abort();
        }
    }

    return res;
}

MachineRange &MachineRange::operator=(const MachineRange &other)
{
    set = other.set;
    return *this;
}

MachineRange &MachineRange::operator=(const MachineRange::ClosedInterval &interval)
{
    set.clear();
    xbt_assert(set.size() == 0);
    set.insert(interval);
    return *this;
}

bool MachineRange::operator==(const MachineRange &other)
{
    return set == other.set;
}

MachineRange & MachineRange::operator&(const MachineRange & other)
{
    return (set & other.set);
}

MachineRange & MachineRange::operator-(const MachineRange &other)
{
    return (set - other.set);
}

MachineRange & MachineRange::operator&=(const MachineRange & other)
{
    set &= other.set;
    return *this;
}

MachineRange & MachineRange::operator-=(const MachineRange &other)
{
    set -= other.set;
    return *this;
}

int MachineRange::operator[](int index) const
{
    xbt_assert(index >= 0 && index < (int)this->size(),
               "Invalid call to MachineRange::operator[]: index (%d) should be in [0,%d[",
               index, (int)this->size());

    // TODO: avoid O(n) retrieval
    auto machine_it = this->elements_begin();
    for (int i = 0; i < index; ++i)
    {
        ++machine_it;
    }

    return *machine_it;
}
