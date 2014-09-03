CPP = ccache g++
#CPP = clang++
CFLAGS += -DDLIB_NO_GUI_SUPPORT 

CFLAGS += -lYap -ldl -lm -lgmp -lmicrohttpd -lpthread -pthread -lcurl -lmysqlclient
CFLAGS += -I./dlib-18.7 -I./include
CFLAGS += -std=c++11
CFLAGS += -Wl,-rpath -Wl,LIBDIR -Wall -Wno-reorder -Wno-sign-compare -Wno-unused-function

CFLAGS += -O3

#CFLAGS += -fno-elide-constructors
#CFLAGS += -fno-inline
#CFLAGS += -g 

#CFLAGS += -pg

CXXFLAGS = ${CFLAGS}

#DLIB_SOURCE = dlib-18.7/dlib/all/source.cpp
	
#KIF_FILE = 3-puzzle.kif
#KIF_FILE = toy.kif
#KIF_FILE = buttons.kif
#KIF_FILE = 2player_normal_form.kif
KIF_FILE = ./kif_repo/tictactoe.kif
#KIF_FILE = ./3pffa.kif

KIF_FILE2 = ./kif_repo/tictactoe
#KIF_FILE2 = ./connectfour
#KIF_FILE2 = 2player_normal_form
#KIF_FILE2 = ./3pffa

KIF_FILE3 = ./kif_repo/jointbuttonsandlights

all: agent muxin flatten

demos: controller_demo parser_demo propnet_demo boot_demo sboot_demo regress regress config_test echo_server
tools: boot_demo sboot_demo regress echo_server

HEADERS = include/agent.h include/message.h include/agent_state.h
HEADERS += include/parser.h include/game.h include/gdl_flatten.h include/gdl_simplify.h
HEADERS += include/controller.h include/yapcontroller.h
HEADERS += include/propnet.h include/propnet_struct.h include/propnet_factory.h
HEADERS += include/player.h include/mcts.h include/mcts_struct.h include/random_player.h include/clasp_player.h
HEADERS += include/slog.h include/common.h include/regress.h
HEADERS += include/config.h

SRCS = src/agent/agent.cc src/agent/message.cc src/agent/agent_state.cc
SRCS += src/parser/game.cc src/parser/parser.cc src/parser/gdl_flatten.cc src/parser/gdl_simplify.cc
SRCS += src/controller/controller.cc src/controller/yapcontroller.cc
SRCS += src/controller/propnet.cc src/controller/propnet_factory.cc
SRCS += src/player/mcts.cc src/player/mcts_node.cc src/player/mcts_aux.cc src/player/mcts_main.cc src/player/mcts_thread.cc
SRCS += src/player/random_player.cc src/player/clasp_player.cc
SRCS += src/slog.cc src/common.cc src/parser/regress.cc

AGENT_MAIN = src/main.cc

OBJECTS = ${SRCS:.cc=.o}

#dlib.o: ${DLIB_SOURCE}
	#${CPP} ${DLIB_SOURCE} -c -o $@

%.o: %.cc ${HEADERS}
	${CPP} ${CFLAGS} -c -o $@ $<

PARSER = gdl_parse.cc gdl_lex.cc
gdl_parse.cc: src/parser/gdl.y
	bison -d src/parser/gdl.y -o $@

gdl_lex.cc: src/parser/gdl.l
	flex -o $@ src/parser/gdl.l

# ===============================================
# DEMOS
# ===============================================

PARSER_DEMO_SRC = demo/parser/main.cc
parser_demo: ${PARSER_DEMO_SRC} ${PARSER} ${HEADERS} ${OBJECTS}
	${CPP} ${PARSER_DEMO_SRC} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS} 

VALGRIND_ARGS = --track-origins=yes --leak-check=full

runvalpd: parser_demo
	valgrind ${VALGRIND_ARGS} ./parser_demo ${KIF_FILE2}

CONTROLLER_DEMO_SRC = demo/controller/main.cc
controller_demo: ${CONTROLLER_DEMO_SRC} ${PARSER} ${HEADERS} ${OBJECTS}
	${CPP} ${CONTROLLER_DEMO_SRC} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS} 

runvalcd: controller_demo
	valgrind ${VALGRIND_ARGS} ./controller_demo ./output

PROPNET_DEMO_SRC = demo/propnet/main.cc
propnet_demo: ${PROPNET_DEMO_SRC} ${PARSER} ${HEADERS} ${OBJECTS}
	${CPP} ${PROPNET_DEMO_SRC} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS}

runvalprop: propnet_demo
	valgrind ${VALGRIND_ARGS} ./propnet_demo ./kif_repo/3puzzle
	#valgrind ${VALGRIND_ARGS} ./propnet_demo ${KIF_FILE2}

FLATTEN_SRC = src/flatten.cc
flatten: ${FLATTEN_SRC} ${OBJECTS} ${PARSER} ${HEADER}
	${CPP} ${FLATTEN_SRC} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS}

FLATTEN_SRC = src/flatten.cc
flatten_test: ${FLATTEN_SRC} ${OBJECTS} ${PARSER} ${HEADER}
	${CPP} ${FLATTEN_SRC} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS}

runvalflat: flatten
	valgrind ${VALGRIND_ARGS}  ./flatten ${KIF_FILE}

REGRESS_DEMO = demo/regress/main.cc
regress: ${REGRESS_DEMO} ${OBJECTS} ${PARSER} ${HEADER}
	${CPP} ${REGRESS_DEMO} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS}

runvalreg: regress
	valgrind ${VALGRIND_ARGS} ./regress ${KIF_FILE}

CONFIG_DEMO = demo/config_test/main.cc
config_test: ${CONFIG_DEMO} include/common.h src/common.cc include/slog.h src/slog.cc
	${CPP} ${CONFIG_DEMO} src/common.cc src/slog.cc -o $@ ${CFLAGS}


# ===============================================
# BOOT TEST PROGRAM 
# ===============================================

BOOT_DEMO_SRC = demo/boot/main.cc
boot_demo: ${BOOT_DEMO_SRC} ${OBJECTS} ${PARSER} ${HEADER}
	${CPP} ${BOOT_DEMO_SRC} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS}

runvalboot: boot_demo
	valgrind ${VALGRIND_ARGS}  ./boot_demo ${KIF_FILE2}

SBOOT_DEMO_SRC = demo/sboot/main.cc
sboot_demo: ${SBOOT_DEMO_SRC} ${OBJECTS} ${PARSER} ${HEADER}
	${CPP} ${SBOOT_DEMO_SRC} ${OBJECTS} ${PARSER} -o $@ ${CFLAGS}

runvalsboot: sboot_demo
	valgrind ${VALGRIND_ARGS}  ./sboot_demo ${KIF_FILE3}

# ===============================================
# AGENT
# ===============================================

agent: ${OBJECTS} ${AGENT_MAIN} ${PARSER} ${HEADERS}
	${CPP} ${OBJECTS} ${PARSER} ${AGENT_MAIN} -o $@ ${CFLAGS}

runval: agent
	valgrind ${VALGRIND_ARGS} --vgdb=yes ./agent

# ===============================================
# MUXIN
# ===============================================

MUXIN_SRC = src/muxin.cc
muxin: ${PARSER} ${MUXIN_SRC} ${HEADERS} ${OBJECTS}
	${CPP} ${MUXIN_SRC} ${PARSER} ${OBJECTS} -o $@ ${CFLAGS} ${CURL_FLAGS}

runvalmux: muxin
	valgrind ${VALGRIND_ARGS} --vgdb=yes ./muxin

ECHO_MAIN = demo/echo_server/main.cc
echo_server: ${ECHO_MAIN}
	${CPP} ${ECHO_MAIN} -o $@ ${CFLAGS}


# ===============================================
# CLEAN
# ===============================================

clean_obj:
	rm -rf ${OBJECTS}

clean_demos:
	rm -rf server_demo{,.dSYM}
	rm -rf parser_demo{,.dSYM}
	rm -rf controller_demo{,.dSYM}
	rm -rf propnet_demo{,.dSYM}
	rm -rf boot_demo{,.dsym}
	rm -rf sboot_demo{,.dsym}
	rm -rf flatten{,.dSYM}
	rm -rf regress{,.dSYM}

clean_parser:
	rm -rf gdl_parse.cc gdl_lex.cc gdl.tab.h gdl_parse.hh

clean: clean_obj clean_demos
	rm -rf gdl_parse.cc gdl_lex.cc gdl.tab.h gdl_parse.hh
	rm -rf agent{,.dSYM}
