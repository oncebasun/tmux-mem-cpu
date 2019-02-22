/*
 * Copyright 2012 Matthew McCormick
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * */

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <unistd.h> // sleep
#include <cmath> // for floorf
#include <cstdlib> // EXIT_SUCCESS

// Apple specific.
#if defined(__APPLE__) && defined(__MACH__)
// Mach kernel includes for getting memory and CPU statistics
# include <mach/vm_statistics.h>
# include <mach/processor_info.h>
# include <mach/mach_types.h>
# include <mach/mach_init.h>
# include <mach/mach_host.h>
# include <mach/host_info.h>
# include <mach/mach_error.h>
# include <mach/vm_map.h>
# include <mach/mach.h>
# include <sys/sysctl.h> // for sysctl
# include <sys/types.h> // for integer types
#endif

// if we are on a BSD system
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
// TODO: Includes and *BSD support
# define BSD_BASED 1
#endif

#if defined(BSD_BASED) || (defined(__APPLE__) && defined(__MACH__))
// OSX or BSD based system, use BSD APIs instead

// See: http://www.opensource.apple.com/source/xnu/xnu-201/osfmk/mach/host_info.h
// and: http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/
host_cpu_load_info_data_t _get_cpu_percentage()
{
  kern_return_t    error;
  mach_msg_type_number_t  count;
  host_cpu_load_info_data_t  r_load;
  mach_port_t    mach_port;

  count = HOST_CPU_LOAD_INFO_COUNT;
  mach_port = mach_host_self();
  error = host_statistics(mach_port, HOST_CPU_LOAD_INFO, (host_info_t)&r_load, &count);
  if (error != KERN_SUCCESS)
  {
    return host_cpu_load_info_data_t();
  }

  return r_load;
}

float cpu_percentage( unsigned int cpu_usage_delay )
{
  // Get the load times from the XNU kernel
  host_cpu_load_info_data_t load1 = _get_cpu_percentage();
  usleep(cpu_usage_delay);
  host_cpu_load_info_data_t load2 = _get_cpu_percentage();

  // Current load times
  unsigned long long current_user    = load1.cpu_ticks[CPU_STATE_USER];
  unsigned long long current_system  = load1.cpu_ticks[CPU_STATE_SYSTEM];
  unsigned long long current_nice    = load1.cpu_ticks[CPU_STATE_NICE];
  unsigned long long current_idle    = load1.cpu_ticks[CPU_STATE_IDLE];
  // Next load times
  unsigned long long next_user       = load2.cpu_ticks[CPU_STATE_USER];
  unsigned long long next_system     = load2.cpu_ticks[CPU_STATE_SYSTEM];
  unsigned long long next_nice       = load2.cpu_ticks[CPU_STATE_NICE];
  unsigned long long next_idle       = load2.cpu_ticks[CPU_STATE_IDLE];
  // Difference between the two
  unsigned long long diff_user       = next_user - current_user;
  unsigned long long diff_system     = next_system - current_system;
  unsigned long long diff_nice       = next_nice - current_nice;
  unsigned long long diff_idle       = next_idle - current_idle;

#else // Linux

float cpu_percentage( unsigned int cpu_usage_delay )
{
  std::string stat_line;
  size_t line_start_pos;
  size_t line_end_pos;
  unsigned long long current_user;
  unsigned long long current_system;
  unsigned long long current_nice;
  unsigned long long current_idle;
  unsigned long long next_user;
  unsigned long long next_system;
  unsigned long long next_nice;
  unsigned long long next_idle;
  unsigned long long diff_user;
  unsigned long long diff_system;
  unsigned long long diff_nice;
  unsigned long long diff_idle;
  std::istringstream iss;

  std::ifstream stat_file("/proc/stat");
  getline(stat_file, stat_line);
  stat_file.close();

  // skip "cpu"
  line_start_pos = stat_line.find_first_not_of(" ", 3);
  line_end_pos = stat_line.find_first_of(' ', line_start_pos);
  line_end_pos = stat_line.find_first_of(' ', line_end_pos + 1);
  line_end_pos = stat_line.find_first_of(' ', line_end_pos + 1);
  line_end_pos = stat_line.find_first_of(' ', line_end_pos + 1);

  iss.str( stat_line.substr( line_start_pos, line_end_pos - line_start_pos ) );
  iss >> current_user >> current_nice >> current_system >> current_idle;
  iss.clear();

  usleep( cpu_usage_delay );

  stat_file.open("/proc/stat");
  getline(stat_file, stat_line);
  stat_file.close();

  // skip "cpu"
  line_start_pos = stat_line.find_first_not_of(" ", 3);
  line_end_pos = stat_line.find_first_of(' ', line_start_pos);
  line_end_pos = stat_line.find_first_of(' ', line_end_pos + 1);
  line_end_pos = stat_line.find_first_of(' ', line_end_pos + 1);
  line_end_pos = stat_line.find_first_of(' ', line_end_pos + 1);

  iss.str( stat_line.substr( line_start_pos, line_end_pos - line_start_pos ) );
  iss >> next_user >> next_nice >> next_system >> next_idle;
  iss.clear();

  diff_user   = next_user - current_user;
  diff_system = next_system - current_system;
  diff_nice   = next_nice - current_nice;
  diff_idle   = next_idle - current_idle;

#endif // platform
  return static_cast<float>(diff_user + diff_system + diff_nice)/static_cast<float>(diff_user + diff_system + diff_nice + diff_idle)*100.0;
}

std::string tick( int percentage )
{
  std::string ticks = "▁▂▃▄▅▆▇█";
  int ticks_count = ticks.size() / 3;
  int tick_pos = (ticks_count * percentage) / 101;
  std::ostringstream oss;

  if (percentage > 75) {
    oss << "#[default]#[fg=red]";
  } else if (percentage > 50) {
    oss << "#[default]#[fg=yellow]";
  } else {
    oss << "#[default]#[fg=green]";
  }
  oss << ticks.substr(tick_pos * 3, 3);
  return oss.str();
}

std::string cpu_string( unsigned int cpu_usage_delay )
{
  float percentage;
  std::ostringstream oss;
  oss.precision( 1 );
  oss.setf( std::ios::fixed | std::ios::right );

  percentage = cpu_percentage( cpu_usage_delay );

  oss.width( 5 );
  oss << percentage << "% ";
  oss << tick(percentage);

  return oss.str();
}

std::string mem_string()
{
  std::ostringstream oss;
#if defined(BSD_BASED) || (defined(__APPLE__) && defined(__MACH__))
  // OSX or BSD based system, use BSD APIs instead

#if defined(__APPLE__) && defined(__MACH__)
  // These values are in bytes
  int64_t total_mem;
  int64_t used_mem;
  int64_t unused_mem;
  vm_size_t page_size;
  mach_port_t mach_port;
  mach_msg_type_number_t count;
  vm_statistics_data_t vm_stats;

  // Get total physical memory
  int mib[2];
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  size_t length = sizeof(int64_t);
  sysctl(mib, 2, &total_mem, &length, NULL, 0);

  mach_port = mach_host_self();
  count = sizeof(vm_stats) / sizeof(natural_t);
  if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
      KERN_SUCCESS == host_statistics(mach_port, HOST_VM_INFO, (host_info_t)&vm_stats, &count))
  {
    unused_mem = (int64_t)vm_stats.free_count * (int64_t)page_size;

    used_mem = ((int64_t)vm_stats.active_count +
        (int64_t)vm_stats.inactive_count +
        (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
  }

  // To kilobytes
#endif // Apple
  // TODO BSD

  used_mem /= 1024;
  total_mem /= 1024;

#else // Linux
  unsigned int total_mem;
  unsigned int used_mem;
  unsigned int unused_mem;
  size_t line_start_pos;
  size_t line_end_pos;
  std::istringstream iss;
  std::string mem_line;

  std::ifstream meminfo_file( "/proc/meminfo" );
  getline( meminfo_file, mem_line );
  line_start_pos = mem_line.find_first_of( ':' );
  line_start_pos++;
  line_end_pos = mem_line.find_first_of( 'k' );
  iss.str( mem_line.substr( line_start_pos, line_end_pos - line_start_pos ) );
  iss >> total_mem;

  used_mem = total_mem;

  while(true) {
    getline( meminfo_file, mem_line );
    if (mem_line.find_first_of( "MemFree" ) != std::string::npos) {
      line_start_pos = mem_line.find_first_of( ':' );
      line_start_pos++;
      line_end_pos = mem_line.find_first_of( 'k' );
      iss.str( mem_line.substr( line_start_pos, line_end_pos - line_start_pos ) );
      iss >> unused_mem;
      used_mem -= unused_mem;
      break;
    }
  }
  meminfo_file.close();
#endif // platform

  oss << used_mem / 1024 << '/' << total_mem / 1024 << "MB ";
  oss << tick(used_mem * 100 / total_mem);
  oss.width( 3 );

  return oss.str();
}

int main(int argc, char** argv)
{
  unsigned int cpu_usage_delay = 900000;
  std::string color = "default";
  try
  {
  std::istringstream iss;
  iss.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
  if( argc > 1 )
    {
      iss.str( argv[1] );
      unsigned int status_interval;
      iss >> status_interval;
      cpu_usage_delay = status_interval * 1000000 - 100000;
    }
  if( argc > 2 )
    {
      iss.str( argv[2] );
      iss.clear();
      iss >> color;
    }
  }
  catch(const std::exception &e)
  {
    std::cerr << "Usage: " << argv[0] << " [tmux_status-interval(seconds)] [color]" << std::endl;
    return 1;
  }

  std::cout << "#[" << color << "] " << mem_string();
  std::cout << "#[" << color << "] " << cpu_string( cpu_usage_delay );

  return 0;
}

