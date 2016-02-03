#include "machine_range.hpp"

#include <vector>

#include <boost/algorithm/string/join.hpp>

#include <simgrid/msg.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(machine_range, "machine_range");

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
}

void MachineRange::insert(MachineRange::Interval interval)
{
    set.insert(interval);
}

void MachineRange::insert(int value)
{
    set.insert(value);
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

std::string MachineRange::to_string_brackets(const std::string & union_str,
                                             const std::string & opening_bracket,
                                             const std::string & closing_bracket,
                                             const std::string & sep)
{
    vector<string> machine_id_strings;
    for (auto it = intervals_begin(); it != intervals_end(); ++it)
        if (it->lower() == it->upper())
            machine_id_strings.push_back(opening_bracket + to_string(it->lower()) + closing_bracket);
        else
            machine_id_strings.push_back(opening_bracket + to_string(it->lower()) + sep + to_string(it->upper()) + closing_bracket);

    return boost::algorithm::join(machine_id_strings, union_str);
}

std::string MachineRange::to_string_hyphen(const std::string &sep, const std::string &joiner)
{
    vector<string> machine_id_strings;
    for (auto it = intervals_begin(); it != intervals_end(); ++it)
        if (it->lower() == it->upper())
            machine_id_strings.push_back(to_string(it->lower()));
        else
            machine_id_strings.push_back(to_string(it->lower()) + joiner + to_string(it->upper()));

    return boost::algorithm::join(machine_id_strings, sep);
}

string MachineRange::to_string_elements(const string &sep)
{
    vector<string> machine_id_strings;
    for (auto it = elements_begin(); it != elements_end(); ++it)
        machine_id_strings.push_back(to_string(*it));

    return boost::algorithm::join(machine_id_strings, sep);
}
