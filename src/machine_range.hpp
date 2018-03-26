/**
 * @file machine_range.hpp
 * @brief Contains the class which handles set of machines
 */

#pragma once

#include <string>

#include <boost/icl/interval_set.hpp>
#include <boost/icl/closed_interval.hpp>

/**
 * @brief Handles a set of machines
 */
struct MachineRange
{
public:
    /**
     * @brief Represents a closed interval between two bounds.
     */
    typedef boost::icl::closed_interval<int, std::less> ClosedInterval;
    /**
     * @brief Represents a Set of ClosedInterval
     */
    typedef boost::icl::interval_set<int,
        ICL_COMPARE_INSTANCE(ICL_COMPARE_DEFAULT, int),
        ClosedInterval> Set;

public:
    /**
     * @brief Returns an element_iterator corresponding to the beginning of the MachineRange
     * @return An element_iterator corresponding to the beginning of the MachineRange
     */
    Set::element_iterator elements_begin();

    /**
     * @brief Returns an element_const_iterator corresponding to the beginning of the MachineRange
     * @return An element_const_iterator corresponding to the beginning of the MachineRange
     */
    Set::element_const_iterator elements_begin() const;

    /**
     * @brief Returns an element_iterator corresponding to the ending of the MachineRange
     * @return An element_iterator corresponding to the ending of the MachineRange
     */
    Set::element_iterator elements_end();

    /**
     * @brief Returns an element_const_iterator corresponding to the ending of the MachineRange
     * @return An element_const_iterator corresponding to the ending of the MachineRange
     */
    Set::element_const_iterator elements_end() const;

    /**
     * @brief Returns an interval iterator corresponding to the beginning of the MachineRange
     * @return An interval iterator corresponding to the beginning of the MachineRange
     */
    Set::iterator intervals_begin();

    /**
     * @brief Returns an interval const_iterator corresponding to the beginning of the MachineRange
     * @return An interval const_iterator corresponding to the beginning of the MachineRange
     */
    Set::const_iterator intervals_begin() const;

    /**
     * @brief Returns an interval iterator corresponding to the ending of the MachineRange
     * @return An interval iterator corresponding to the ending of the MachineRange
     */
    Set::iterator intervals_end();

    /**
     * @brief Returns an interval const_iterator corresponding to the ending of the MachineRange
     * @return An interval const_iterator corresponding to the ending of the MachineRange
     */
    Set::const_iterator intervals_end() const;

    /**
     * @brief Clears the MachineRange: After this call, it will be empty
     */
    void clear();

    /**
     * @brief Inserts a MachineRange into another MachineRange
     * @param[in] range The MachineRange to insert
     */
    void insert(const MachineRange & range);

    /**
     * @brief Inserts a ClosedInterval into a MachineRange
     * @param[in] interval The interval to insert
     */
    void insert(ClosedInterval interval);

    /**
     * @brief Inserts a value (a single machine) into a MachineRange
     * @param[in] value The value (the single machine) to insert
     */
    void insert(int value);

    /**
     * @brief Removes a MachineRange from another MachineRange
     * @param[in] range The MachineRange to remove
     */
    void remove(const MachineRange & range);

    /**
     * @brief Removes a ClosedInterval from a MachineRange
     * @param[in] interval The ClosedInterval to remove
     */
    void remove(ClosedInterval interval);

    /**
     * @brief Removes a value (a single machine) from a MachineRange
     * @param value The value (the single machine) to remove
     */
    void remove(int value);

    /**
     * @brief Returns the first element of the MachineRange
     * @return The first element of the MachineRange
     */
    int first_element() const;

    /**
     * @brief Returns the number of machines in the MachineRange
     * @return The number of machines in the MachineRange
     */
    unsigned int size() const;

    /**
     * @brief Returns whether a machine is in the MachineRange
     * @param[in] machine_id The machine
     * @return True if and only if the the given machine is in the MachineRange
     */
    bool contains(int machine_id) const;

    /**
     * @brief Returns a std::string corresponding to the MachineRange
     * @details For example, with default values, the MachineRange {1,2,3,7} would be outputted as "[1,3]∪[7]"
     * @param[in] union_str The union string
     * @param[in] opening_bracket The opening_bracket string
     * @param[in] closing_bracket The closing_bracket string
     * @param[in] sep The separator string
     * @return A std::string corresponding to the MachineRange
     */
    std::string to_string_brackets(const std::string & union_str = "∪", const std::string & opening_bracket = "[", const std::string & closing_bracket = "]", const std::string & sep = ",") const;
    /**
     * @brief Returns a std::string corresponding to the MachineRange
     * @details For example, with default values, the MachineRange {1,2,3,7} would be outputted as "1-3,7"
     * @param[in] sep The separator string
     * @param[in] joiner The joiner string
     * @return A std::string corresponding to the MachineRange
     */
    std::string to_string_hyphen(const std::string & sep = ",", const std::string & joiner = "-") const;

    /**
     * @brief Returns a std::string corresponding to the MachineRange
     * @details For example, with default values, the MachineRange {1,2,3,7} would be outputted as "1,2,3,7"
     * @param sep The separator string
     * @return A std::string corresponding to the MachineRange
     */
    std::string to_string_elements(const std::string & sep = ",") const;

    /**
     * @brief Sets the set of a MachineRange such that it is equal to the set of another MachineRange
     * @param[in] other The other MachineRange
     * @return The current MachineRange after the operation
     */
    MachineRange & operator=(const MachineRange & other);

    /**
     * @brief Sets the set of a MachineRange such that it is equal to another ClosedInterval
     * @param[in] interval The ClosedInterval
     * @return The current MachineRange after the operation
     */
    MachineRange & operator=(const MachineRange::ClosedInterval & interval);

    /**
     * @brief Returns whether a MachineRange is equal to another MachineRange
     * @param[in] other The other MachineRange
     * @return True if and only if the two MachineRange contain the same machines
     */
    bool operator==(const MachineRange & other);

    /**
     * @breif Returns the intersection of two MachineRange
     */
    //MachineRange & operator&(const MachineRange & other);

    /**
     * @breif Returns the difference of two MachineRange
     */
    //MachineRange & operator-(const MachineRange & other);

    /**
     * @brief Sets the set as the intersection between itself and another MachineRange
     * @param[in] other The other MachineRange
     * @return The current MachineRange after the operation
     */
    MachineRange & operator &=(const MachineRange & other);

    /**
     * @brief Sets the set as the difference between itself and another MachineRange
     * @param[in] other The other MachineRange
     * @return The current MachineRange after the operation
     */
    MachineRange & operator-=(const MachineRange & other);

    /**
     * @brief Returns the index-th machine of the MachineRange
     * @param[in] index The 0-based index of the machine to retrieve
     * @return The machine at the index-th position of the MachineRange
     */
    int operator[](int index) const;

public:
    /**
     * @brief Creates a MachineRange from a hyphenized string
     * @details Hyphenized strings are a kind of representation of sets of machines. For example, with default values of sep and joiner, "1-3,7" represents the machines {1,2,3,7}.
     * @param[in] str The hyphenized string
     * @param[in] sep The separator string
     * @param[in] joiner The joiner string
     * @param[in] error_prefix The error prefix (used to output errors)
     * @return The MachineRange which corresponds to a hyphenized string
     */
    static MachineRange from_string_hyphen(const std::string & str,
                                           const std::string & sep = ",",
                                           const std::string & joiner = "-",
                                           const std::string & error_prefix = "Invalid machine range string");

    Set set; //!< The internal set of machines
};


MachineRange & difference(const MachineRange & one, const MachineRange & other);

MachineRange & intersection(const MachineRange & one, const MachineRange & other);
