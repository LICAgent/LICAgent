#ifndef __CONFIG_H__
#define __CONFIG_H__

//just macros, the rest can be found in the configure file

//#define MCTS_DEBUG
//#define MCTS_COLLECT_ERROR
//#define MCTS_CHECK

//#define GDL_FLATTEN_DEBUG

#define AGENT_USE_PROPNET

#define AGENT_USE_CLASP

#define YAP_USE_FASTINIT

#define CONFIG_FILE "sen.config"

#define XSTR(x) STR(x)
#define STR(x) #x

#define SCONFIG_FILE XSTR(CONFIG_FILE)

#endif
