SRC=$(wildcard *.cpp)
OBJS=$(SRC:%.cpp=%.o)
TEST=$(SRC:%.cpp=%.test)

snapshot_io.test halo_io.test: HDFLIB=
$(TEST): ../config_parser.o ../mymath.o
halo_io.test: ../halo.o ../snapshot.o snapshot_io.o
snapshot_io.test: ../snapshot.o

HBTDIR=../..
include $(HBTDIR)/Makefile.inc