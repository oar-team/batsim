#include "periodic.hpp"

#include <simgrid/s4u.hpp>

void generate_static_periodic_schedule(
  const std::vector<PeriodicTrigger*> & triggers,
  std::vector<TimeSlice> & schedule)
{

}

void periodic_main_actor()
{
  auto mbox_recv = simgrid::s4u::Mailbox::by_name("periodic");
}
