#include "eudaq/Monitor.hh"
#include "eudaq/StandardEvent.hh"
#include "eudaq/StdEventConverter.hh"
#include "eudaq/Configuration.hh"
#include "eudaq/Utils.hh"
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>
#include <thread>
#include <random>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iterator>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <regex>
#include <filesystem>
#include <algorithm>

#include <sys/inotify.h>
/* // TODO: For cross platform monitoring of new files in folder need to adapt code with
#include <QFileSystemWatcher>
*/

struct CorryArgumentList {
  char **argv;
  size_t sz, used;
} ;


struct DataCollectorAttributes {
  std::string name;
  std::string eventloader_type;
  std::vector<std::string> detector_planes;
  std::string fwpatt;

  std::pair<std::string, std::string> full_file;
  std::string monitor_file_path;
  std::string pattern_to_match;
  std::string event_name;

  bool found_matching_file = false;

} ;


#define TOKEN " "
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

//----------DOC-MARK-----BEG*DEC-----DOC-MARK----------
class CorryMonitor : public eudaq::Monitor {
public:
  CorryMonitor(const std::string & name, const std::string & runcontrol);
  void DoInitialise() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoTerminate() override;
  void DoReset() override;
  void DoReceive(eudaq::EventSP ev) override;
  
  static const uint32_t m_id_factory = eudaq::cstr2hash("CorryMonitor");
  
private:
  std::pair<std::string, std::string> getFileString(std::string pattern);

  bool m_en_print;
  bool m_en_std_converter;
  bool m_en_std_print;
  pid_t m_corry_pid;
  std::vector<std::string> m_datacollectors_to_monitor; 
  std::vector<std::string> m_eventloader_types;
  std::string m_datacollector_to_monitor;
  std::string m_eventloader_type;
  std::string m_corry_path;
  std::string m_corry_config;
  std::string m_corry_options;
  std::vector<std::string> m_detector_planes;

  std::string m_fwpatt;
  std::string m_fwtype;

  CorryArgumentList m_args;

  std::string m_test_string;
  std::string m_test_string_2;
  std::vector<DataCollectorAttributes> m_datacollector_vector;

};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Monitor>::
    Register<CorryMonitor, const std::string&, const std::string&>(CorryMonitor::m_id_factory);
}

CorryMonitor::CorryMonitor(const std::string & name, const std::string & runcontrol)
  :eudaq::Monitor(name, runcontrol){  
}

void CorryMonitor::DoInitialise(){
  auto ini = GetInitConfiguration();
  ini->Print(std::cout);
  m_corry_path = ini->Get("CORRY_PATH", "/path/to/corry");
  
  struct stat buffer;   
  if(stat(m_corry_path.c_str(), &buffer) != 0)
    EUDAQ_ERROR("Corryvreckan cannot be found under "+m_corry_path+" ! Please check your /path/to/corry (Avoid using ~)");

}

// Store execvp() arguments in char array according to https://stackoverflow.com/questions/29484366/how-to-make-execvp-handle-multiple-arguments
static char **addArg (char **argv, size_t *pSz, size_t *pUsed, char *str) {
    // Make sure enough space for another one.

    if (*pUsed == *pSz) {
        *pSz = *pSz + 25;
        argv = (char **) realloc (argv, *pSz * sizeof (char*));
        if (argv == 0) {
            std::cerr << "Out of memory\n";
            exit (1);
        }
    }

    // Add it and return (possibly new) array.

    argv[(*pUsed)++] = (str == 0) ? 0 : strdup (str);
    return argv;
}

unsigned int countDigit(long long n)
{
    if (n/10 == 0)
        return 1;
    return 1 + countDigit(n / 10);
}

// String matching with wildcards taken from https://stackoverflow.com/questions/23457305/compare-strings-with-wildcard
bool string_match(const char *pattern, const char *candidate, int p, int c) {
  if (pattern[p] == '\0') {
    return candidate[c] == '\0';
  } else if (pattern[p] == '*') {
    for (; candidate[c] != '\0'; c++) {
      if (string_match(pattern, candidate, p+1, c))
        return true;
    }
    return string_match(pattern, candidate, p+1, c);
  } else if (pattern[p] != '?' && pattern[p] != candidate[c]) {
    return false;
  }  else {
    return string_match(pattern, candidate, p+1, c+1);
  }
}

std::pair<std::string, std::string> CorryMonitor::getFileString(std::string pattern) {
  // Decrypt file pattern. Can't use file namer because we need to know position of date/time

  std::regex reg("\\$([0-9]*)(D|R|X)");

  std::sregex_iterator iter(pattern.begin(), pattern.end(), reg);
  std::sregex_iterator end;

  std::string file_string = "";

  uint32_t run_number = GetRunNumber();
  unsigned int run_number_digits = countDigit(run_number);
  std::string run_number_str = std::to_string(run_number);

  std::string time_placeholder(1, '*');

  std::string suffix;

  while (iter!=end){

    file_string += (*iter).prefix();

    // number is numerical value attached to the letter in the file pattern
    // e.g. 12 for $12D
    uint16_t number (((*iter)[1] == "") ? 0 : std::stoi((*iter)[1]) );
    //std::cout<< "Number is " << std::to_string(number) << " while iter is " << (*iter)[1] << std::endl;

    std::string letter = (*iter)[2];

    if (letter == "D"){
      file_string += time_placeholder;
    }
    else if (letter == "R") {
      unsigned int leading_zeros((number>run_number_digits) ? number-run_number_digits : 0);
      file_string += std::string(leading_zeros, '0')+run_number_str;
    }
    else if (letter == "X") {
      file_string += ".raw";
    }

    // Overwrite suffix until final element in iter is reached
    suffix = (*iter).suffix();

    ++iter;
  }

  file_string += suffix;

  
  EUDAQ_DEBUG("File string for matching is " + file_string);

  
  //std::regex return_regex(file_string);

  std::filesystem::path file_path(file_string);


  return std::pair<std::string, std::string>(file_path.parent_path(), file_path.filename());

}


void CorryMonitor::DoConfigure(){
  auto conf = GetConfiguration();
  //conf->Print(std::cout);
  m_en_print                  = conf->Get("CORRY_ENABLE_PRINT", 0);
  m_en_std_converter          = conf->Get("CORRY_ENABLE_STD_CONVERTER", 0);
  m_en_std_print              = conf->Get("CORRY_ENABLE_STD_PRINT", 0);
  m_datacollector_to_monitor  = conf->Get("DATACOLLECTOR_TO_MONITOR", "my_dc");
  m_eventloader_type          = conf->Get("CORRESPONDING_EVENTLOADER_TYPE", "");
  m_corry_config              = conf->Get("CORRY_CONFIG_PATH", "placeholder.conf");
  m_corry_options             = conf->Get("CORRY_OPTIONS", "");

  m_test_string               = conf->Get("TEST_STRING", "test1, test2");
  m_test_string_2             = conf->Get("TEST_STRING_2", "test4, test3");

  // Check if corryvreckan is found
  struct stat buffer;   
  if(stat(m_corry_config.c_str(), &buffer) != 0)
    EUDAQ_ERROR("Config for corry cannot be found under "+m_corry_config+" ! Please check your /path/to/config.conf (Avoid using ~)");


  // command to be exectued in DoStartRun(), stored tokenized in m_args.argv
  std::string my_command = m_corry_path + " -c " + m_corry_config + " " + m_corry_options;

  //    Initial size, used and array.
  m_args.argv = 0;
  m_args.sz = 0;
  m_args.used = 0;

  char * cstr = new char[my_command.length()+1];
  std::strcpy(cstr, my_command.c_str());

  // Add the command itself.
  m_args.argv = addArg (m_args.argv, &m_args.sz, &m_args.used, strtok (cstr, TOKEN));

  // Add each argument in turn, then the terminator (added later in DoStartRun()).
  while ((cstr = strtok (0, TOKEN)) != 0){
        m_args.argv = addArg (m_args.argv, &m_args.sz, &m_args.used, cstr);
  }

  // open corry config file to get geometry file
  std::ifstream corry_file {m_corry_config};
  std::shared_ptr<eudaq::Configuration> corry_conf = std::make_shared<eudaq::Configuration>(corry_file, "Corryvreckan");
  //corry_conf->Print();

  // open geometry file (exploit same file structure for geometry file as for config file)
  std::ifstream geo_file {corry_conf->Get("detectors_file", "")};
  std::shared_ptr<eudaq::Configuration> corry_geo = std::make_shared<eudaq::Configuration>(geo_file, "");
  //corry_geo->Print();


  // Test out best way to split string
  std::stringstream ss(m_test_string);
  while( ss.good() )
  {
      std::string substr;
      getline( ss, substr, ',' );
      m_datacollectors_to_monitor.push_back( eudaq::trim(substr) );
  }

  // Test out best way to split string
  std::stringstream ss2(m_test_string_2);
  while( ss2.good() )
  {
      std::string substr;
      getline( ss2, substr, ',' );
      m_eventloader_types.push_back( eudaq::trim(substr) );
  }



  // Get the file naming pattern from the DataCollector config section
  std::string section = "DataCollector."+m_datacollector_to_monitor;
  std::string eudaq_config_file_path = conf->Name();

  // Check if DataCollector with name m_datacollector_to_monitor is found
  conf->SetSection("");
  if (!(conf->Has(section)))
    EUDAQ_THROW("DataCollector to be monitored (\"" + section + "\") not found!");
  else 
    EUDAQ_DEBUG("DataCollector to be monitored is " + section);

  std::ifstream eudaq_conf {eudaq_config_file_path};

  // open eudaq config file and get the DataCollector section
	std::shared_ptr<eudaq::Configuration> dc_conf = std::make_shared<eudaq::Configuration>(eudaq_conf, section);
  //dc_conf->Print();

  m_fwpatt = dc_conf->Get("EUDAQ_FW_PATTERN", "$12D_run$6R$X"); // Default value hard-coded. Must be same as in DataCollector.cc

  

  //TODO: check if using lcase() from Utils works too
  // transform EvetLoader type to lower case letters
  std::transform(m_eventloader_type.cbegin(), m_eventloader_type.cend(),
                   m_eventloader_type.begin(), // write to the same location
                   [](unsigned char c) { return std::tolower(c); });
  
  // loop over all detector planes and save the ones which match m_eventloader_type
  // needed to pass file to be monitored to corry at runtime
  for (auto m: corry_geo->Sectionlist()){
    corry_geo->SetSection(m);
    if (corry_geo->Get("type","") == m_eventloader_type){
      m_detector_planes.push_back(m);
    }
  }
  
  for (auto s: m_eventloader_types)
  {
    std::cout<<s<<std::endl;
  }

  
  //std::ifstream eudaq_conf {eudaq_config_file_path};
  for (int i=0; i<m_datacollectors_to_monitor.size(); i++)
  {
    //std::string key;
    DataCollectorAttributes value;
    value.name = m_datacollectors_to_monitor[i];
    // Get the file naming pattern from the DataCollector config section
    std::string section_2 = "DataCollector."+value.name;
    std::string eudaq_config_file_path = conf->Name();

    // Check if DataCollector with name m_datacollector_to_monitor is found
    conf->SetSection("");
    if (!(conf->Has(section_2)))
      EUDAQ_THROW("DataCollector to be monitored (\"" + section_2 + "\") not found!");
    else 
      EUDAQ_DEBUG("DataCollector to be monitored is " + section_2);

    
    // ifstream needs to be newly created for each conf (declare in loop)
    std::ifstream eudaq_conf_2 {eudaq_config_file_path};
    // open eudaq config file and get the DataCollector section
    auto dc_conf_2 = new eudaq::Configuration(eudaq_conf_2, section_2);
    std::cout<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<std::endl;
    std::cout<<"SECTION IS " <<section_2<<std::endl;
    dc_conf_2->Print();
    std::cout<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<std::endl;

    value.fwpatt = dc_conf_2->Get("EUDAQ_FW_PATTERN", "$12D_run$6R$X"); // Default value hard-coded. Must be same as in DataCollector.cc
    delete(dc_conf_2);

    

    //TODO: check if using lcase() from Utils works too
    // transform EvetLoader type to lower case letters
    //std::transform(m_eventloader_type.cbegin(), m_eventloader_type.cend(),
    //                m_eventloader_type.begin(), // write to the same location
    //                [](unsigned char c) { return std::tolower(c); });
    value.eventloader_type = eudaq::lcase(m_eventloader_types[i]);
    
    // loop over all detector planes and save the ones which match m_eventloader_type
    // needed to pass file to be monitored to corry at runtime
    for (auto m: corry_geo->Sectionlist()){
      corry_geo->SetSection(m);
      if (corry_geo->Get("type","") == value.eventloader_type){
        value.detector_planes.push_back(m);
      }
    }

    m_datacollector_vector.push_back(value);

  }
  
  for ( auto const& it : m_datacollector_vector)
  {
    std::cout<<"###############################~~##########################~~~###~~~##~~~##~~###~~###~~~#~~~~##"<<std::endl;
    std::cout<<it.name<<" : " <<it.eventloader_type << " " << it.detector_planes[0] << std::endl;
  }


}

void CorryMonitor::DoStartRun(){

  int fd, wd; // File descriptor and watch descriptor for inotify

  std::pair<std::string, std::string> full_file = getFileString(m_fwpatt);
  std::string monitor_file_path((full_file.first=="") ? "./" : full_file.first+"/");
  std::string pattern_to_match = full_file.second;
  std::string event_name;


  for (auto & it : m_datacollector_vector)
  {
    // can only call getFileString after run has started because of GetRunNumber()
    it.full_file = getFileString(it.fwpatt);
    it.monitor_file_path = std::string((it.full_file.first=="") ? "./" : it.full_file.first+"/");
    it.pattern_to_match = it.full_file.second;
  }

    for ( auto const& it : m_datacollector_vector)
  {
    std::cout<<"###############################~~##########################~~~###~~~##~~~##~~###~~###~~~#~~~~##"<<std::endl;
    std::cout<<it.name<<" : " <<it.monitor_file_path << " " << it.fwpatt << " " << it.pattern_to_match << std::endl;
  }

  bool waiting_for_matching_file = true;

  bool all_wait_true = false;

  m_corry_pid = fork();

  switch (m_corry_pid)
  {
  case -1: // error
    perror("fork");
    exit(1);

  case 0: // child: start corryvreckan
    
    // Setting up inotify
    fd = inotify_init();
    if ( fd < 0 ) {
      perror( "Couldn't initialize inotify");
    }

    wd = inotify_add_watch(fd, monitor_file_path.c_str(), IN_CREATE);

    while(waiting_for_matching_file){

      // reading the event (change in directory)
      int length, i = 0;
      char buffer[BUF_LEN];

      length = read( fd, buffer, BUF_LEN );
      if ( length < 0 ) {
        perror( "read" );
      }  

      // loop over changes in directory and check if any of them is creation of desired file
      while ( i < length ) {
        struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
      
        if ( event->mask & IN_CREATE ) {      // if event is a creation of object in directory
          if ( !(event->mask & IN_ISDIR) ) {  // if object created is a file
            if ( event->len ) {               // if filename is not empty 
              std::stringstream ss;
              ss << event->name;
              event_name = ss.str();

              EUDAQ_DEBUG("The file " + event_name + " was created");
              //EUDAQ_DEBUG("Pattern to match is  " + pattern_to_match); 

              for (auto & it : m_datacollector_vector){
                if (it.found_matching_file == false){
                  EUDAQ_DEBUG("Testing pattern " + it.pattern_to_match);
                  if (string_match(it.pattern_to_match.c_str(), event_name.c_str(), 0, 0)) {
                    EUDAQ_DEBUG("Found a match with pattern " + it.pattern_to_match);
                    it.found_matching_file = true;
                    break;
                  }
                }
              }

              all_wait_true = std::all_of(m_datacollector_vector.begin(), m_datacollector_vector.end(), [](const auto& v) {
                    return v.found_matching_file;
                });

              if (all_wait_true) {
                std::cout<<":::::::::::::::::::::::::::::::::::::::::::::::::::::::::FINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFINFIFNFINFINFINFINFINFINFIN"<<std::endl;
                waiting_for_matching_file =false;
              }


              
              //if (string_match(pattern_to_match.c_str(), event_name.c_str(), 0, 0)) 
              //  waiting_for_matching_file = false;
            }
          }
        }

        i += EVENT_SIZE + event->len;

      }

    }

    // Found file name is now stored in event_name
    EUDAQ_INFO("File to be monitored is "+monitor_file_path+event_name);

    // add passing the file name to corry to the command
    for (auto m: m_detector_planes){
      std::string my_command = "-o EventLoaderEUDAQ2:"+m+".file_name="+monitor_file_path+event_name;
      char * cstr = new char[my_command.length()+1];
      std::strcpy(cstr, my_command.c_str());

      // Add the command itself.
      m_args.argv = addArg (m_args.argv, &m_args.sz, &m_args.used, strtok (cstr, TOKEN));

      // Add each argument in turn, then the terminator.
      while ((cstr = strtok (0, TOKEN)) != 0){
        m_args.argv = addArg (m_args.argv, &m_args.sz, &m_args.used, cstr);
      }
    }

    m_args.argv = addArg (m_args.argv, &m_args.sz, &m_args.used, 0);

    /*
    for (const auto & entry : std::filesystem::directory_iterator("/home/andreas/Documents/eudaq/user/example/misc/")){
        std::cout << entry.path().filename() << std::endl;
        std::cout << "Is this a match? " << std::string((string_match(pattern_to_match.c_str(), entry.path().filename().c_str(), 0, 0)) ? "Yes" : "No") << std::endl;
    }
    */

    execvp(m_args.argv[0], m_args.argv);
    perror("execv"); // execv doesn't return unless there is a problem
    exit(1);
  
  default: // parent
    break;
  }
  
}

// Killing child process (corry) (adapted from https://stackoverflow.com/questions/13273836/how-to-kill-child-of-fork)
void CorryMonitor::DoStopRun(){
  kill(m_corry_pid, SIGINT);

  bool died = false;
  for (int loop=0; !died && loop < 5; ++loop)
  {
    int status;
    eudaq::mSleep(1000);
    if (waitpid(m_corry_pid, &status, WNOHANG) == m_corry_pid) died = true;
  }

  if (!died) kill(m_corry_pid, SIGQUIT);
}

void CorryMonitor::DoReset(){
}

void CorryMonitor::DoTerminate(){
}

void CorryMonitor::DoReceive(eudaq::EventSP ev){
  if(m_en_print)
    ev->Print(std::cout);
  if(m_en_std_converter){
    auto stdev = std::dynamic_pointer_cast<eudaq::StandardEvent>(ev);
    if(!stdev){
      stdev = eudaq::StandardEvent::MakeShared();
      eudaq::StdEventConverter::Convert(ev, stdev, nullptr); //no conf
      if(m_en_std_print)
	stdev->Print(std::cout);
    }
  }
}
