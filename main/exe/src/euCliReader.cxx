#include "eudaq/OptionParser.hh"
#include "eudaq/FileReader.hh"
#include "eudaq/StdEventConverter.hh"

#include <iostream>

int main(int /*argc*/, const char **argv) {
  eudaq::OptionParser op("EUDAQ Command Line FileReader modified for TLU", "2.1", "EUDAQ FileReader (TLU)");
  eudaq::Option<std::string> file_input(op, "i", "input", "", "string", "input file");
  eudaq::Option<std::string> file_conf(op, "c", "config", "", "string", "configuration file");
  eudaq::Option<uint32_t> eventl(op, "e", "event", 0, "uint32_t", "event number low");
  eudaq::Option<uint32_t> eventh(op, "E", "eventhigh", 0, "uint32_t", "event number high");
  eudaq::Option<uint32_t> triggerl(op, "tg", "trigger", 0, "uint32_t", "trigger number low");
  eudaq::Option<uint32_t> triggerh(op, "TG", "triggerhigh", 0, "uint32_t", "trigger number high");
  eudaq::Option<uint32_t> timestampl(op, "ts", "timestamp", 0, "uint32_t", "timestamp low");
  eudaq::Option<uint32_t> timestamph(op, "TS", "timestamphigh", 0, "uint32_t", "timestamp high");
  eudaq::OptionFlag stat(op, "s", "statistics", "enable print of statistics");
  eudaq::OptionFlag stdev(op, "std", "stdevent", "enable converter of StdEvent");
  eudaq::OptionFlag bianca(op, "b", "bianca", "a thing for biancas testbeam 2024");

  op.Parse(argv);
  std::string infile_path = file_input.Value();
  std::string type_in = infile_path.substr(infile_path.find_last_of(".")+1);
  if(type_in=="raw")
    type_in = "native";

  bool stdev_v = stdev.Value();
  bool stat_v = stat.Value();
  bool bianca_v = bianca.Value();
  if (bianca_v)
    stat_v = true;

  uint32_t eventl_v = eventl.Value();
  uint32_t eventh_v = eventh.Value();
  uint32_t triggerl_v = triggerl.Value();
  uint32_t triggerh_v = triggerh.Value();
  uint32_t timestampl_v = timestampl.Value();
  uint32_t timestamph_v = timestamph.Value();
  bool not_all_zero = eventl_v||eventh_v||triggerl_v||triggerh_v||timestampl_v||timestamph_v;


  // load configuration
    // empty configuration object, prevents crash if no config file given
  eudaq::Configuration eu_cfg( "", "" );
  std::ifstream conffile( file_conf.Value() );
  if( conffile ){
    eu_cfg.Load( conffile, std::string("euCliReader") );
  }
  // prevent warning if no config file name given
  else if ( file_conf.Value().empty() ){}
  // but warn if attempt failed
  else{
    std::cout << "WARNING, config file '" << file_conf.Value() << "' not found!" << std::endl;
  }
  // build shared pointer
  const eudaq::Configuration const_eu_cfg = eu_cfg;
  eudaq::ConfigurationSPC config_spc = std::make_shared<const eudaq::Configuration>(const_eu_cfg);


  eudaq::FileReaderUP reader;
  reader = eudaq::Factory<eudaq::FileReader>::MakeUnique(eudaq::str2hash(type_in), infile_path);
  uint32_t event_count = 0;
  // for the gathering of statistics - initialise with end-of-range values
  std::map<uint32_t,std::string> device_list;
  uint32_t tgn_low = std::numeric_limits<uint32_t>::max();
  uint32_t tgn_high = 0;
  std::map<uint32_t,uint32_t> low_list;
  std::map<uint32_t,uint32_t> high_list;
  std::map<uint32_t,uint32_t> nogood;
  
  while(1){
    auto ev = reader->GetNextEvent();
    if(!ev)
      break;
    bool in_range_evn = false;
    if(eventl_v!=0 || eventh_v!=0){
      uint32_t ev_n = ev->GetEventN();
      if(ev_n >= eventl_v && ev_n < eventh_v){
        in_range_evn = true;
      }
    }
    else
      in_range_evn = true;

    bool in_range_tgn = false;
    if(triggerl_v!=0 || triggerh_v!=0){
      uint32_t tg_n = ev->GetTriggerN();
      if(tg_n >= triggerl_v && tg_n < triggerh_v){
        in_range_tgn = true;
      }
    }
    else
      in_range_tgn = true;

    bool in_range_tsn = false;
    if(timestampl_v!=0 || timestamph_v!=0){
      uint32_t ts_beg = ev->GetTimestampBegin();
      uint32_t ts_end = ev->GetTimestampEnd();
      if(ts_beg >= timestampl_v && ts_end <= timestamph_v){
        in_range_tsn = true;
      }
    }
    else
      in_range_tsn = true;


    if((in_range_evn && in_range_tgn && in_range_tsn) && not_all_zero){
      ev->Print(std::cout);
      if(stdev_v){
        auto evstd = eudaq::StandardEvent::MakeShared();
        eudaq::StdEventConverter::Convert(ev, evstd, config_spc);
        std::cout<< ">>>>>"<< evstd->NumPlanes() <<"<<<<"<<std::endl;
      }
    }

    if(stat_v){
      auto ev_sub_evts = ev->GetSubEvents();
      for (auto subev : ev_sub_evts){
        uint32_t stg_n = subev->GetTriggerN();
	uint32_t s_stream_n = subev->GetStreamN();
        // emplace only inserts if key does not exist yet
        device_list.emplace(s_stream_n, subev->GetDescription());
        if (stg_n>=0 && stg_n<tgn_low)
	  tgn_low=stg_n;	
        if (stg_n>=0 && stg_n>tgn_high)
	  tgn_high=stg_n;

	if(bianca_v){	    
	  // c++17 - insert_or_assign clobbers existing ones
	  try {
	    uint32_t lastlow = low_list.at(s_stream_n);
	    if (stg_n>=0 && stg_n<lastlow)
	      low_list.insert_or_assign(s_stream_n, stg_n);
	  }
	  catch (const std::out_of_range&) {
	    // std::cout << "No entry yet in low map for device stream number " << s_stream_n << std::endl;
	    // then kick of the list with the current one
	    low_list.insert_or_assign(s_stream_n, stg_n);
	  }
	  
	  try {
	    uint32_t lasthigh = high_list.at(s_stream_n);
	    if (stg_n>=0 && stg_n>lasthigh)
	      high_list.insert_or_assign(s_stream_n, stg_n);
	  }
	  catch (const std::out_of_range&) {
	    //std::cout << "No entry yet in high map for device stream number " << s_stream_n << std::endl;
	    high_list.insert_or_assign(s_stream_n, stg_n);
	  }

	  // now it get's extra hacky:
	  // every device _should_ have trigger number almost equal event_number in cms tb december 24
	  if (abs(subev->GetEventN() - stg_n) > 2){
	    try {
	      uint32_t firstnogood = nogood.at(s_stream_n);
	    }
	    catch (const std::out_of_range&) {
	      // subtract 2 to be safe (aka I don't want to think about which offset they have...)
	      nogood.emplace(s_stream_n, stg_n - 2);
	    }	    
	  }
	}
      }
    }
    event_count ++;
  }
  std::cout<< "There are "<< event_count << " events"<<std::endl;

  if(stat_v){
    if (tgn_low!=std::numeric_limits<uint32_t>::max() && tgn_high!=0)
      std::cout<< "Trigger numbers found from "<< tgn_low << " to " << tgn_high <<std::endl;
    std::cout<< "Devices found:" <<std::endl;    
    for (const auto& device : device_list){
      std::cout << device.first << " => " << device.second << std::endl;
    }
  }
  if(bianca_v){
    std::cout<< "Lowest trigger number of device:" <<std::endl;    
    for (const auto& low : low_list){
      std::cout << low.first << " => " << low.second << std::endl;
    }    
    std::cout<< "Highest trigger number of device:" <<std::endl;    
    for (const auto& high : high_list){
      std::cout << high.first << " => " << high.second << std::endl;
    }    
    for (const auto& nope : nogood){
      if (nope.second != 0){
	std::cout<< "This is where it breaks:" <<std::endl;    
	std::cout << nope.first << " => " << nope.second << std::endl;
      }
    }    
  }
  
  return 0;
}
