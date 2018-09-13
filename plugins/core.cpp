#include <positronbot.hpp>

#include <functional>
#include <regex>
#include <sstream>
#include <thread>

using namespace std::placeholders;

template<typename Out>
static void split_str(const std::string& s, char delim, Out result) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}

static std::vector<std::string> split_str(const std::string& s, char delim) {
  std::vector<std::string> e;
  split_str(s, delim, std::back_inserter(e));
  return e;
}

static std::string join_strings(std::vector<std::string> v, char delim = ' ', int start = 0) {
  std::stringstream ss;
  int s = v.size();
  for (int i = start; i < s; ++i) {
    ss << v[i];
    if (i != (s - 1)) {
      ss << delim;
    }
  }
  return ss.str();
}

using namespace std::regex_constants;

static std::regex regex_code("^:.+? (\\d\\d\\d) .+? (.+)$", ECMAScript | icase);
static std::regex regex_ping("^PING :(.+)$", ECMAScript | icase);
static std::regex regex_join("^:(.+?)!(.+?)@(.+?) JOIN (.+)$", ECMAScript | icase);
static std::regex regex_part("^:(.+?)!(.+?)@(.+?) PART (.+?) :(.+)$", ECMAScript | icase);
static std::regex regex_privmsg("^:(.+?)!(.+?)@(.+?) PRIVMSG (.+?) :(.+)$", ECMAScript | icase);
static std::regex regex_notice("^:(.+?)!(.+?)@(.+?) NOTICE (.+?) :(.+)$", ECMAScript | icase);
static std::regex regex_nick("^:(.+?)!(.+?)@(.+?) NICK :(.+)$", ECMAScript | icase);

extern "C" {

  pb::plugin* pb_plugin;

  void pb_init(std::vector<std::pair<std::string, std::string>>& cfg) {
    pb_plugin->l.debug("Hello, World!");
    
    pb_plugin->register_event_handler<pb::event_test>([] (pb::event::ptr _e) { 
      pb::event_test::ptr e = pb_ptrcast<pb::event_test>(_e);
      pb_plugin->l.debug("Received a test event");
    });
    
    pb_plugin->register_event_handler<pb::event_disconnect>([] (pb::event::ptr _e) {
      pb::event_disconnect::ptr e = pb_ptrcast<pb::event_disconnect>(_e);
      std::this_thread::sleep_for(std::chrono::seconds(5));
      std::thread(std::bind(&pb::ircsocket_base::connect, e->socket)).detach();
    });
    
    pb_plugin->register_event_handler<pb::event_message>([] (pb::event::ptr _e) {
      pb::event_message::ptr e = pb_ptrcast<pb::event_message>(_e);
      
      auto m = std::smatch {};
      
      if ((m = std::smatch{}, std::regex_match(e->message, m, regex_privmsg))) {
        std::string nick = m[1], user = m[2], host = m[3], target = m[4], message = m[5];
        e->pbot->emit(pb::event_privmsg::create(e->pbot, e->socket, nick, user, host, target, message));
      } else if ((m = std::smatch{}, std::regex_match(e->message, m, regex_notice))) {
        std::string nick = m[1], user = m[2], host = m[3], target = m[4], message = m[5];
        e->pbot->emit(pb::event_notice::create(e->pbot, e->socket, nick, user, host, target, message));
      } else if ((m = std::smatch{}, std::regex_match(e->message, m, regex_code))) {
        int code = std::stoi(m[1]); std::string extra = m[2];
        e->pbot->emit(pb::event_code::create(e->pbot, e->socket, code, extra));
      } else if ((m = std::smatch{}, std::regex_match(e->message, m, regex_ping))) {
        std::string target = m[1];
        e->pbot->emit(pb::event_ping::create(e->pbot, e->socket, target));
      } else if ((m = std::smatch{}, std::regex_match(e->message, m, regex_join))) {
        std::string nick = m[1], user = m[2], host = m[3], channel = m[4];
        e->pbot->emit(pb::event_join::create(e->pbot, e->socket, nick, user, host, channel));
      } else if ((m = std::smatch{}, std::regex_match(e->message, m, regex_part))) {
        std::string nick = m[1], user = m[2], host = m[3], channel = m[4], reason = m[5];
        e->pbot->emit(pb::event_part::create(e->pbot, e->socket, nick, user, host, channel, reason));
      } else if ((m = std::smatch{}, std::regex_match(e->message, m, regex_nick))) {
        std::string nick = m[1], user = m[2], host = m[3], new_nick = m[4];
        e->pbot->emit(pb::event_nick::create(e->pbot, e->socket, nick, user, host, new_nick));
      }
    });
    
    pb_plugin->register_event_handler<pb::event_ping>([] (pb::event::ptr _e) {
      pb::event_ping::ptr e = pb_ptrcast<pb::event_ping>(_e);
      e->socket->stream() << ("PONG :" + e->target + "\r\n");
    });
    
    pb_plugin->register_event_handler<pb::event_code>([] (pb::event::ptr _e) {
      pb::event_code::ptr e = pb_ptrcast<pb::event_code>(_e);
      if (e->code == 1) {
        e->socket->autojoin();
      } else if (e->code == 352) {
        static std::regex regex_whoreply("^.+? (.+?) (.+?) .+? (.+?) .*");
        auto m = std::smatch {};
        if (std::regex_match(e->extra, m, regex_whoreply)) {
          std::string nick = m[3], user = m[1], host = m[2];
          e->pbot->emit(pb::event_whoreply::create(e->pbot, e->socket, nick, user, host));
        }
      }
    });
    
    pb_plugin->register_event_handler<pb::event_whoreply>([] (pb::event::ptr _e) {
      pb::event_whoreply::ptr e = pb_ptrcast<pb::event_whoreply>(_e);
      auto& uc = e->socket->get_user_cache();
      uc[e->nick].nick = e->nick;
      uc[e->nick].user = e->user;
      uc[e->nick].host = e->host;
    });
    
    pb_plugin->register_event_handler<pb::event_nick>([] (pb::event::ptr _e) {
      pb::event_nick::ptr e = pb_ptrcast<pb::event_nick>(_e);
      if (e->nick == e->socket->get_config().nick) {
        e->socket->get_config().nick = e->nick;
      }
      auto& uc = e->socket->get_user_cache();
      uc[e->new_nick].nick = e->new_nick;
      uc[e->new_nick].user = e->user;
      uc[e->new_nick].host = e->host;
      uc[e->nick] = pb::user();
    });
    
    pb_plugin->register_event_handler<pb::event_join>([] (pb::event::ptr _e) {
      pb::event_join::ptr e = pb_ptrcast<pb::event_join>(_e);
      
      if (e->nick == e->socket->get_config().nick) {
        e->socket->stream() << pb::ircstream::who(e->channel);
      }
      
      auto& uc = e->socket->get_user_cache();
      uc[e->nick].nick = e->nick;
      uc[e->nick].user = e->user;
      uc[e->nick].host = e->host;
    });
    
    pb_plugin->register_event_handler<pb::event_privmsg>([] (pb::event::ptr _e) {
      pb::event_privmsg::ptr e = pb_ptrcast<pb::event_privmsg>(_e);
    
      auto& uc = e->socket->get_user_cache();
      uc[e->nick].nick = e->nick;
      uc[e->nick].user = e->user;
      uc[e->nick].host = e->host;
      
      std::string prefix = e->pbot->get_prefix();
      if (e->message.size() > prefix.size() && e->message.substr(0, prefix.size()) == prefix) {
        std::string message = e->message.substr(prefix.size(), e->message.size()-1);
        std::vector<std::string> split = split_str(message, ' ');
        std::string cmd = split[0];
        
        e->pbot->emit(pb::event_command::create(e->pbot, e->socket, e->nick, e->user, e->host, e->target, cmd, split));
      } else if (e->message[0] == '\x01' && e->message[e->message.size()-1] == '\x01') {
        std::string message = e->message.substr(1, e->message.size()-2);
        std::vector<std::string> split = split_str(message, ' ');
        std::string type = split[0];
        std::string data = join_strings(split, ' ', 1);
        
        e->pbot->emit(pb::event_ctcp::create(e->pbot, e->socket, e->nick, e->user, e->host, e->target, type, data));
      }
    });
    
    pb_plugin->register_event_handler<pb::event_ctcp>([] (pb::event::ptr _e) {
      pb::event_ctcp::ptr e = pb_ptrcast<pb::event_ctcp>(_e);
      if (e->type == "VERSION") {
        e->socket->stream() << pb::ircstream::nctcp(e->nick, "VERSION", "PositronBot - An IRC bot in C++ - https://github.com/handicraftsman/positronbot/");
      }
    });
    
    pb_plugin->register_event_handler<pb::event_notice>([] (pb::event::ptr _e) {
      pb::event_notice::ptr e = pb_ptrcast<pb::event_notice>(_e);
      
      auto& uc = e->socket->get_user_cache();
      uc[e->nick].nick = e->nick;
      uc[e->nick].user = e->user;
      uc[e->nick].host = e->host;
      
      if (e->message[0] == '\x01' && e->message[e->message.size()-1] == '\x01') {
        std::string message = e->message.substr(1, e->message.size()-2);
        std::vector<std::string> split = split_str(message, ' ');
        std::string type = split[0];
        std::string data = join_strings(split, ' ', 1);
        
        e->pbot->emit(pb::event_nctcp::create(e->pbot, e->socket, e->nick, e->user, e->host, e->target, type, data));
      }
    });
    
    pb_plugin->register_event_handler<pb::event_command>([] (pb::event::ptr _e) {
      pb::event_command::ptr e = pb_ptrcast<pb::event_command>(_e);
      
      particledi::dm_ptr dm = e->pbot->get_dm();
      std::shared_ptr<pb::plugin_service> pl_s = dm->get<pb::plugin_service>();
      
      pl_s->handle_command(e);
    });
    
    pb::command cmd_ping {
      .pplugin     = pb_plugin,
      .name        = "ping",
      .usage       = "",
      .description = "ping!",
      .cooldown    = 10,
      .flag        = "world",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        e->socket->stream() << pb::ircstream::reply(e, "pong");
      }
    };
    pb_plugin->register_command(cmd_ping);
    
    pb::command cmd_nick {
      .pplugin     = pb_plugin,
      .name        = "nick",
      .usage       = "<new-nickname>",
      .description = "changes nickname",
      .cooldown    = 0,
      .flag        = "nick",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        if (e->split.size() != 2) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        e->socket->stream() << pb::ircstream::nick(e->split[1]);
      }
    };
    pb_plugin->register_command(cmd_nick);
    
    pb::command cmd_join {
      .pplugin     = pb_plugin,
      .name        = "join",
      .usage       = "<channel> [password]",
      .description = "joins channels",
      .cooldown    = 0,
      .flag        = "joinpart",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        if (e->split.size() == 2) {
          e->socket->stream() << pb::ircstream::join(e->split[1]);
        } else if (e->split.size() == 3) {
          e->socket->stream() << pb::ircstream::join(e->split[1], e->split[2]);
        } else {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
        }
      }
    };
    pb_plugin->register_command(cmd_join);
    
    pb::command cmd_part {
      .pplugin     = pb_plugin,
      .name        = "part",
      .usage       = "<channel> [password]",
      .description = "parts (leaves) channels",
      .cooldown    = 0,
      .flag        = "joinpart",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        if (e->split.size() == 2) {
          e->socket->stream() << pb::ircstream::part(e->split[1]);
        } else if (e->split.size() > 2) {
          std::stringstream reason;
          for (int i = 2; i < e->split.size(); ++i) {
            reason << e->split[i] << " ";
          }
          e->socket->stream() << pb::ircstream::part(e->split[1], reason.str());
        } else {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
        }
      }
    };
    pb_plugin->register_command(cmd_part);
    
    pb::command cmd_key {
      .pplugin     = pb_plugin,
      .name        = "key",
      .usage       = "| <key>",
      .description = "Generates a key when no arguments are provided. Gives you admin permissions when you type correct key",
      .cooldown    = 0,
      .flag        = "world",
      .handler     = [pb_plugin] (pb::command& c, pb::event_command::ptr e) {
        static char* key = nullptr;
        
        if (e->split.size() == 1) {
          const int sz = 64;
          if (key != nullptr) delete[] key;
          key = new char[sz + 1];
                    
          static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
          ;
          
          srand(time(NULL));
          for (int i = 0; i < sz; ++i) {
            key[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
          }

          key[sz] = '\0';
          
          pb_plugin->l.debug("Your key: %s", key);
          e->socket->stream() << pb::ircstream::nreply(e, "Done!");
        } else if (e->split.size() == 2) {
          if (key == nullptr || (std::string(key) != e->split[1])) {
            e->socket->stream() << pb::ircstream::nreply(e, "Invalid key!");
          } else {
            particledi::dm_ptr dm = e->pbot->get_dm();
            std::shared_ptr<pb::db_service> db_s = dm->get<pb::db_service>();
            
            pb::flag f(e->socket->get_name());
            f.host    = e->host;
            
            db_s->insert(f);
            
            e->socket->stream() << pb::ircstream::nreply(e, "Done!");
          }
          delete[] key;
          key = nullptr;
        } else {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
        }
      }
    };
    pb_plugin->register_command(cmd_key);
    
    auto flag_handler = [] (pb::command& c, pb::event_command::ptr e) {
      particledi::dm_ptr dm = e->pbot->get_dm();
      std::shared_ptr<pb::db_service> db_s = dm->get<pb::db_service>();
      
      if (e->cmd == "flag-list") {
        if (e->split.size() != 6) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        
        pb::flag f(e->split[1]);
        f.channel = e->split[2] == "*" ? std::nullopt : std::optional(e->split[2]);
        f.host    = e->split[3] == "*" ? std::nullopt : std::optional(e->split[3]);
        f.plugin  = e->split[4] == "*" ? std::nullopt : std::optional(e->split[4]);
        f.name    = e->split[5] == "*" ? std::nullopt : std::optional(e->split[5]);
        
        std::vector<pb::flag> flags = db_s->list(f);
        
        pb::ircstream s = e->socket->stream();
        for (pb::flag f : flags) {
          s << pb::ircstream::nreply(
            e,
            "> server=[" + f.server +
            "] channel=[" + f.channel.value_or("<*>") +
            "] host=[" + f.host.value_or("<*>") +
            "] plugin=[" + f.plugin.value_or("<*>") +
            "] name=[" + f.name.value_or("<*>") +
            "]"
          );
        }
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      } else if (e->cmd == "flag-insert") {
        if (e->split.size() != 6) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        
        pb::flag f(e->split[1]);
        f.channel = e->split[2] == "*" ? std::nullopt : std::optional(e->split[2]);
        f.host    = e->split[3] == "*" ? std::nullopt : std::optional(e->split[3]);
        f.plugin  = e->split[4] == "*" ? std::nullopt : std::optional(e->split[4]);
        f.name    = e->split[5] == "*" ? std::nullopt : std::optional(e->split[5]);
        
        db_s->insert(f);
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      } else if (e->cmd == "flag-remove") {
        if (e->split.size() != 6) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        
        pb::flag f(e->split[1]);
        f.channel = e->split[2] == "*" ? std::nullopt : std::optional(e->split[2]);
        f.host    = e->split[3] == "*" ? std::nullopt : std::optional(e->split[3]);
        f.plugin  = e->split[4] == "*" ? std::nullopt : std::optional(e->split[4]);
        f.name    = e->split[5] == "*" ? std::nullopt : std::optional(e->split[5]);
        
        db_s->remove(f);
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      } else if (e->cmd == "flag-givec") {
        if (e->split.size() != 4) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        pb::flag f(e->socket->get_name());
        f.channel = e->split[1];
        f.plugin  = e->split[2];
        f.name    = e->split[3];
        db_s->insert(f);
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      } else if (e->cmd == "flag-takec") {
        if (e->split.size() != 4) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        pb::flag f(e->socket->get_name());
        f.channel = e->split[1];
        f.plugin  = e->split[2];
        f.name    = e->split[3];
        db_s->remove(f);
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      } else if (e->cmd == "flag-giveh") {
        if (e->split.size() != 4) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        pb::flag f(e->socket->get_name());
        f.host   = e->split[1];
        f.plugin = e->split[2];
        f.name   = e->split[3];
        db_s->insert(f);
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      } else if (e->cmd == "flag-takeh") {
        if (e->split.size() != 4) {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
          return;
        }
        pb::flag f(e->socket->get_name());
        f.host   = e->split[1];
        f.plugin = e->split[2];
        f.name   = e->split[3];
        db_s->remove(f);
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      }
    };
    
    pb::command cmd_flag_list {
      .pplugin     = pb_plugin,
      .name        = "flag-list",
      .usage       = "<server> <channel|*> <host|*> <plugin|*> <name|*>",
      .description = "lists flags",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = flag_handler
    };
    pb_plugin->register_command(cmd_flag_list);
    
    pb::command cmd_flag_insert {
      .pplugin     = pb_plugin,
      .name        = "flag-insert",
      .usage       = "<server> <channel|*> <host|*> <plugin|*> <name|*>",
      .description = "inserts given flag into the database",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = flag_handler
    };
    pb_plugin->register_command(cmd_flag_insert);
    
    pb::command cmd_flag_remove {
      .pplugin     = pb_plugin,
      .name        = "flag-remove",
      .usage       = "<server> <channel|*> <host|*> <plugin|*> <name|*>",
      .description = "removes given flag from the database",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = flag_handler
    };
    pb_plugin->register_command(cmd_flag_remove);
    
    pb::command cmd_flag_givec {
      .pplugin     = pb_plugin,
      .name        = "flag-givec",
      .usage       = "<channel> <plugin> <name>",
      .description = "gives given flag to the given channel",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = flag_handler
    };
    pb_plugin->register_command(cmd_flag_givec);
    
    pb::command cmd_flag_takec {
      .pplugin     = pb_plugin,
      .name        = "flag-takec",
      .usage       = "<channel> <plugin> <name>",
      .description = "takes given flag from the given channel",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = flag_handler
    };
    pb_plugin->register_command(cmd_flag_takec);
    
    pb::command cmd_flag_giveh {
      .pplugin     = pb_plugin,
      .name        = "flag-giveh",
      .usage       = "<host> <plugin> <name>",
      .description = "gives given flag to the given hostname",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = flag_handler
    };
    pb_plugin->register_command(cmd_flag_giveh);
    
    pb::command cmd_flag_takeh {
      .pplugin     = pb_plugin,
      .name        = "flag-takeh",
      .usage       = "<host> <plugin> <name>",
      .description = "takes given flag from the given hostname",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = flag_handler
    };
    pb_plugin->register_command(cmd_flag_takeh);
    
    auto group_handler = [] (pb::command& c, pb::event_command::ptr e) {
      std::map<std::string, pb::config_group_t> groups = e->pbot->get_groups();
      
      if (c.name == "group-list") {
        if (e->split.size() == 1) {
          std::stringstream res;
          res << "&BGroups:&N ";
          for (std::pair<std::string, pb::config_group_t> g : groups) {
            res << g.first << " ";
          }
          e->socket->stream() << pb::ircstream::nreply(e, res.str());
        } else if (e->split.size() == 2) {
          if (groups.find(e->split[1]) == groups.end()) {
            e->socket->stream() << pb::ircstream::nreply(e, "No such group!");
            return;
          }
          pb::config_group_t& group = groups[e->split[1]];
          std::stringstream res;
          res << "&B" << e->split[1] << ":&N ";
          for (std::pair<std::string, std::string> p : group.flags) {
            res << p.first << "/" << p.second << " ";
          }
          e->socket->stream() << pb::ircstream::nreply(e, res.str());
        } else {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
        }
      }
    };
    
    enum class GroupManipWhat {
      None,
      Channel,
      Hostname
    };
    
    enum class GroupManipTask {
      Give,
      Take
    };
    
    auto group_manip_handler = [] (
      int args,
      GroupManipWhat what,
      GroupManipTask task,
      pb::command& c,
      pb::event_command::ptr e
    ) {
      particledi::dm_ptr dm = e->pbot->get_dm();
      std::shared_ptr<pb::db_service> db_s = dm->get<pb::db_service>();
      
      std::map<std::string, pb::config_group_t> groups = e->pbot->get_groups();
      
      std::optional<std::string> channel;
      std::optional<std::string> host;
      std::string gname;
      
      if (args == 1) {
        if (what == GroupManipWhat::Channel) {
          channel = e->split[1];
        } else if (what == GroupManipWhat::Hostname) {
          host = e->split[1];
        }
        gname = e->split[2];
      } else if (args == 2) {
        channel = e->split[1];
        host    = e->split[2];
        gname   = e->split[3];
      } else {
        e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
        return;
      }
      
      if (groups.find(gname) == groups.end()) {
        e->socket->stream() << pb::ircstream::nreply(e, "No such group!");
        return;
      }
      
      pb::config_group_t& group = groups[gname];
      
      int n = 0;
      if (task == GroupManipTask::Give) {
        for (std::pair<std::string, std::string> p : group.flags) {
          pb::flag f(e->socket->get_name());
          f.channel = channel;
          f.host    = host;
          f.plugin  = p.first;
          f.name    = p.second;
          db_s->insert(f);
          ++n;
        }
      } else if (task == GroupManipTask::Take) {
        for (std::pair<std::string, std::string> p : group.flags) {
          pb::flag f(e->socket->get_name());
          f.channel = channel;
          f.host    = host;
          f.plugin  = p.first;
          f.name    = p.second;
          db_s->remove(f);
          ++n;
        }        
      }
      
      e->socket->stream() << pb::ircstream::nreply(e, "Done, " + std::to_string(n) + " flags modified!");
    };
    
    pb::command cmd_group_list {
      .pplugin     = pb_plugin,
      .name        = "group-list",
      .usage       = "| <group>",
      .description = "lists groups and their flags",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = group_handler
    };
    pb_plugin->register_command(cmd_group_list);
    
    pb::command cmd_group_give {
      .pplugin     = pb_plugin,
      .name        = "group-give",
      .usage       = "<channel> <hostname> <group>",
      .description = "gives groups",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = std::bind(group_manip_handler, 2, GroupManipWhat::None, GroupManipTask::Give, _1, _2)
    };
    pb_plugin->register_command(cmd_group_give);
    
    pb::command cmd_group_take {
      .pplugin     = pb_plugin,
      .name        = "group-take",
      .usage       = "<channel> <hostname> <group>",
      .description = "takes groups",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = std::bind(group_manip_handler, 2, GroupManipWhat::None, GroupManipTask::Take, _1, _2)
    };
    pb_plugin->register_command(cmd_group_take);
    
    pb::command cmd_group_givec {
      .pplugin     = pb_plugin,
      .name        = "group-givec",
      .usage       = "<channel> <group>",
      .description = "gives given group to the given channel",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = std::bind(group_manip_handler, 1, GroupManipWhat::Channel, GroupManipTask::Give, _1, _2)
    };
    pb_plugin->register_command(cmd_group_givec);
    
    pb::command cmd_group_takec {
      .pplugin     = pb_plugin,
      .name        = "group-takec",
      .usage       = "<channel> <group>",
      .description = "takes given group from the given channel",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = std::bind(group_manip_handler, 1, GroupManipWhat::Channel, GroupManipTask::Take, _1, _2)
    };
    pb_plugin->register_command(cmd_group_takec);
    
    pb::command cmd_group_giveh {
      .pplugin     = pb_plugin,
      .name        = "group-giveh",
      .usage       = "<hostname> <group>",
      .description = "gives given group to the given hostname",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = std::bind(group_manip_handler, 1, GroupManipWhat::Hostname, GroupManipTask::Give, _1, _2)
    };
    pb_plugin->register_command(cmd_group_giveh);
    
    pb::command cmd_group_takeh {
      .pplugin     = pb_plugin,
      .name        = "group-takeh",
      .usage       = "<hostname> <group>",
      .description = "takes given group from the given hostname",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = std::bind(group_manip_handler, 1, GroupManipWhat::Hostname, GroupManipTask::Take, _1, _2)
    };
    pb_plugin->register_command(cmd_group_takeh);
    
    pb::command cmd_help {
      .pplugin = pb_plugin,
      .name        = "help",
      .usage       = "| <command>",
      .description = "shows help messages for commands",
      .cooldown    = 0,
      .flag        = "world",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        particledi::dm_ptr dm = e->pbot->get_dm();
        std::shared_ptr<pb::plugin_service> pl_s = dm->get<pb::plugin_service>();
        
        if (e->split.size() == 1) {
          e->socket->stream() << pb::ircstream::nreply(
            e,
            "Type &B" + e->pbot->get_prefix() + "help&N to show this message. " +
            "Type &B" + e->pbot->get_prefix() + "help <command>&N to show info about a command. " +
            "Type &B" + e->pbot->get_prefix() + "list&N to list all plugins. " +
            "Type &B" + e->pbot->get_prefix() + "list <plugin>&N to list all commands in a plugin. "
          );
        } else if (e->split.size() == 2) {
          for (std::pair<std::string, pb::plugin::ptr> p : pl_s->plugins) {
            if (p.second->commands.find(e->split[1]) != p.second->commands.end()) {
              pb::command& c = p.second->commands[e->split[1]];
              e->socket->stream() << pb::ircstream::nreply(
                e,
                "> " +
                p.first + "/" + c.name + " | " +
                c.name + " " + c.usage + " | " +
                c.description + " | cooldown=" +
                std::to_string(c.cooldown) + " | flag=[" +
                p.first + "/" + c.flag + "]"
              );
            }
          }
        } else {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
        }
      }
    };
    pb_plugin->register_command(cmd_help);
    
    pb::command cmd_list {
      .pplugin     = pb_plugin,
      .name        = "list",
      .usage       = "| <plugin>",
      .description = "list plugins and their commands",
      .cooldown    = 0,
      .flag        = "world",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        particledi::dm_ptr dm = e->pbot->get_dm();
        std::shared_ptr<pb::plugin_service> pl_s = dm->get<pb::plugin_service>();
        
        if (e->split.size() == 1) {
          std::stringstream ss;
          ss << "&BPlugins:&N ";
          
          for (std::pair<std::string, pb::plugin::ptr> p : pl_s->plugins) {
            ss << p.first << " ";
          }
          
          e->socket->stream() << pb::ircstream::nreply(e, ss.str());
        } else if (e->split.size() == 2 && (pl_s->plugins.find(e->split[1]) != pl_s->plugins.end())) {
          std::stringstream ss;
          ss << "&B" << e->split[1] << ":&N ";
          
          for (std::pair<std::string, pb::command> p : pl_s->plugins[e->split[1]]->commands) {
            ss << p.first << " ";
          }
          
          e->socket->stream() << pb::ircstream::nreply(e, ss.str());
        } else {
          e->socket->stream() << pb::ircstream::nreply(e, "Invalid arguments!");
        }
      }
    };
    pb_plugin->register_command(cmd_list);
    
    pb::command cmd_raw {
      .pplugin     = pb_plugin,
      .name        = "raw",
      .usage       = "<message...>",
      .description = "sends given message to the server",
      .cooldown    = 0,
      .flag        = "admin",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        std::stringstream ss;
        for (int i = 1; i < e->split.size(); ++i) {
          ss << e->split[i] << " ";
        }
        e->socket->stream() << ss.str() << "\r\n";
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      }
    };
    pb_plugin->register_command(cmd_raw);
    
    pb::command cmd_flushq {
      .pplugin     = pb_plugin,
      .name        = "flushq",
      .usage       = "",
      .description = "flushes the queue",
      .cooldow     = 0,
      .flag        = "world",
      .handler     = [] (pb::command& c, pb::event_command::ptr e) {
        e->socket->flushq();
        e->socket->stream() << pb::ircstream::nreply(e, "Done!");
      }
    };
    pb_plugin->register_command(cmd_flushq);
  }
  
  void pb_deinit() {
    pb_plugin->l.debug("Bye, World!");
  }

}

