#pragma once

#include <string>

#include <boost/icl/interval_set.hpp>
#include <boost/icl/closed_interval.hpp>

struct MachineRange
{
public:
    typedef boost::icl::interval_set<int> Set;
    typedef Set::interval_type Interval;

public:
    Set set;

public:
    Set::element_iterator elements_begin();
    Set::element_const_iterator elements_begin() const;

    Set::element_iterator elements_end();
    Set::element_const_iterator elements_end() const;

    Set::iterator intervals_begin();
    Set::const_iterator intervals_begin() const;

    Set::iterator intervals_end();
    Set::const_iterator intervals_end() const;

    void clear();
    void insert(const MachineRange & range);
    void insert(Interval interval);
    void insert(int value);

    void remove(const MachineRange & range);
    void remove(Interval interval);
    void remove(int value);

    MachineRange left(int nb_machines) const;

    int first_element() const;
    unsigned int size() const;
    bool contains(int machine_id) const;

    std::string to_string_brackets(const std::string & union_str = "∪", const std::string & opening_bracket = "[", const std::string & closing_bracket = "]", const std::string & sep = ",") const;
    std::string to_string_hyphen(const std::string & sep = ",", const std::string & joiner = "-") const;
    std::string to_string_elements(const std::string & sep = ",") const;

    MachineRange & operator=(const MachineRange & other);
    MachineRange & operator=(const MachineRange::Interval & interval);

    bool operator==(const MachineRange & other);
    MachineRange & operator&=(const MachineRange & other);
    MachineRange &operator-=(const MachineRange & other);

public:
    static MachineRange from_string_hyphen(const std::string & str, const std::string & sep = ",", const std::string & joiner = "-", const std::string & error_prefix = "Invalid machine range string");
};
