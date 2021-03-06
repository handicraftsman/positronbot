#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <particledi.hpp>

#include "event.hpp"
#include "plugin.hpp"

namespace pb {

  class bot;
  class bot_private;
  
  class plugin_service : public particledi::dependency {
  public:
    friend bot;
    friend bot_private;
  
    typedef std::shared_ptr<plugin_service> ptr;
  
    plugin_service(particledi::dm_ptr dm, const std::vector<std::string>& plugin_dirs);
    virtual ~plugin_service();

    std::optional<plugin::ptr> get(std::string& name);

    void load_plugin(std::string& name, std::vector<std::pair<std::string, std::string>>& cfg);
    
    void emit(event::ptr e);

    void handle_command(event_command::ptr e);
    
    particledi::dm_ptr dm;
    std::map<std::string, plugin::ptr> plugins;
    std::vector<std::string> plugin_dirs;
  };
}
