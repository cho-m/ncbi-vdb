# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

default: runtests

TOP ?= $(abspath ../..)

MODULE = test/vdb

INT_TOOLS = \
	check-user-agent-header-in-thread \

TEST_TOOLS = \
	test-vdb \
	test-wvdb \
	test-dependencies \
	test-sparse-col-rw \
	test-sparse-col-rd \
	test-blob-val \
	test-VDB-3060 \
	test-VDB-3061 \
	test-bytecode \
	test-view \
	test-tablecursor \
	test-wtablecursor \
	test-view-cursor \
	test-VDB-4156   \

#	test-VDB-3418

SLOW_TEST_TOOLS =  \
	test-vdb-slow \
	test-wvdb-slow \

ALL_TOOLS = $(TEST_TOOLS) $(INT_TOOLS)

# override global configuration
VDB_CONFIG = $(SRCDIR)/..

include $(TOP)/build/Makefile.env

$(ALL_TOOLS) $(SLOW_TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

runtests: setup

setup:
	@ mkdir -p db

.PHONY: $(TEST_TOOLS) $(SLOW_TEST_TOOLS) setup

clean: stdclean

READ_LIBS = \
	-skapp \
	-sktst \
	-sncbi-vdb

WRITE_LIBS = \
	-skapp \
	-sktst \
	-sncbi-wvdb

#-------------------------------------------------------------------------------
# test-vdb
#
TEST_VDB_SRC = \
	test-vdb

TEST_VDB_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_VDB_SRC))

$(TEST_BINDIR)/test-vdb: $(TEST_VDB_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

vdb: test-vdb
	$(TEST_BINDIR)/test-vdb

valgrind_vdb: std
	valgrind --ncbi --show-reachable=no $(TEST_BINDIR)/test-vdb

#-------------------------------------------------------------------------------
# test-dependencies
#
TEST_DEPS_SRC = \
	test-dependencies

TEST_DEPS_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_DEPS_SRC))

$(TEST_BINDIR)/test-dependencies: $(TEST_DEPS_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

deps: test-dependencies
	$(TEST_BINDIR)/test-dependencies

valgrind_deps: std
	valgrind --ncbi --show-reachable=no $(TEST_BINDIR)/test-dependencies

#-------------------------------------------------------------------------------
# test-wvdb
#
TEST_WVDB_SRC = \
	test-wvdb

TEST_WVDB_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_WVDB_SRC))

$(TEST_BINDIR)/test-wvdb: $(TEST_WVDB_OBJ)
	$(LP) --exe -o $@ $^ $(WRITE_LIBS)

# make sure these targets do not pick up the global schema
wvdb: test-wvdb
	VDB_CONFIG=$(VDB_CONFIG) $(TEST_BINDIR)/test-wvdb

valgrind_wvdb: std
	VDB_CONFIG=$(VDB_CONFIG) valgrind --ncbi --show-reachable=no $(TEST_BINDIR)/test-wvdb

#-------------------------------------------------------------------------------
# test-sparse-col
#  We will compile the same test file first in read/write mode, create DB and
#  run tests using libncbi-wvdb library; then compile the same test with -DREAD_ONLY
#  and link it against read-only lincbi-vdb library
TEST_SPARSE_COL_SRC = \
	test-sparse-col

TEST_SPARSE_COL_OBJ_RW = \
	$(addsuffix .$(OBJX),$(TEST_SPARSE_COL_SRC)-rw)

TEST_SPARSE_COL_OBJ_RD = \
	$(addsuffix .$(OBJX),$(TEST_SPARSE_COL_SRC)-rd)

TEST_SPARSE_COL_LIB = \
	-skapp \
	-sktst \

$(TEST_SPARSE_COL_OBJ_RW): $(TEST_SPARSE_COL_SRC).cpp
	$(CP) -o $@ $(OPT) $< $(LOC_INFO) -D__file_ext__=cpp

$(TEST_SPARSE_COL_OBJ_RD): $(TEST_SPARSE_COL_SRC).cpp
	$(CP) -o $@ $(OPT) $< $(LOC_INFO) -D__file_ext__=cpp -DREAD_ONLY

$(TEST_BINDIR)/test-sparse-col-rw: $(TEST_SPARSE_COL_OBJ_RW)
	$(LP) --exe -o $@ $^ $(TEST_SPARSE_COL_LIB) -sncbi-wvdb

$(TEST_BINDIR)/test-sparse-col-rd: $(TEST_SPARSE_COL_OBJ_RD)
	$(LP) --exe -o $@ $^ $(TEST_SPARSE_COL_LIB) -sncbi-vdb

sparse-col: test-sparse-col-rw test-sparse-col-rd
	$(TEST_BINDIR)/test-sparse-col-rw
	$(TEST_BINDIR)/test-sparse-col-rd

valgrind_sparse-col: std
	valgrind --ncbi --show-reachable=no $(TEST_BINDIR)/test-sparse-col-rw
	valgrind --ncbi --show-reachable=no $(TEST_BINDIR)/test-sparse-col-rd

#-------------------------------------------------------------------------------
# test-blob-val
#
TEST_BLOB_VAL_SRC = \
	test-blob-val

TEST_BLOB_VAL_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_BLOB_VAL_SRC))

$(TEST_BINDIR)/test-blob-val: $(TEST_BLOB_VAL_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

blob-val: test-blob-val
	$(TEST_BINDIR)/test-blob-val

#-------------------------------------------------------------------------------
CHTS_SRC = \
	check-user-agent-header-in-thread

CHTS_OBJ = \
	$(addsuffix .$(OBJX),$(CHTS_SRC))

CHTS_LIB = \
	-sncbi-vdb

$(TEST_BINDIR)/check-user-agent-header-in-thread: $(CHTS_OBJ)
	$(LP) --exe -o $@ $^ $(CHTS_LIB)

user-agent-check: check-user-agent-header-in-thread
	$(TEST_BINDIR)/check-user-agent-header-in-thread

#-------------------------------------------------------------------------------
# SYS-224064
#
SYS_224064_SRC = \
	SYS-224064

SYS_224064_OBJ = \
	$(addsuffix .$(OBJX),$(SYS_224064_SRC))

SYS_224064_LIB = \
	-sncbi-vdb \

$(TEST_BINDIR)/SYS-224064: $(SYS_224064_OBJ)
	$(LP) --exe -o $@ $^ $(SYS_224064_LIB)

SYS-224064: makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@


#-------------------------------------------------------------------------------
# VDB-3060
#
VDB_3060_SRC = \
	test-VDB-3060

VDB_3060_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_3060_SRC))

$(TEST_BINDIR)/test-VDB-3060: $(VDB_3060_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

VDB-3060: test-VDB-3060
	$(TEST_BINDIR)/test-VDB-3060

#-------------------------------------------------------------------------------
# VDB-3061
#
VDB_3061_SRC = \
	test-VDB-3061

VDB_3061_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_3061_SRC))

$(TEST_BINDIR)/test-VDB-3061: $(VDB_3061_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

VDB-3061: test-VDB-3061
	$(TEST_BINDIR)/test-VDB-3061

#-------------------------------------------------------------------------------
# byte code
#
VDB_BYTECODE_SRC = \
	test-bytecode

VDB_BYTECODE_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_BYTECODE_SRC))

VDB_BYTECODE_LIB = \
	-skapp \
	-sktst \
	-sncbi-wvdb

$(TEST_BINDIR)/test-bytecode: $(VDB_BYTECODE_OBJ)
	$(LP) --exe -o $@ $^ $(VDB_BYTECODE_LIB)

bytecode: test-bytecode
	$(TEST_BINDIR)/test-bytecode

#-------------------------------------------------------------------------------
# v2 View
#
VDB_VIEW_SRC = \
	test-view

VDB_VIEW_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_VIEW_SRC))

VDB_VIEW_LIB = \
	-skapp \
	-sktst \
	-sncbi-wvdb \
	-sschema

$(TEST_BINDIR)/test-view: $(VDB_VIEW_OBJ)
	$(LP) --exe -o $@ $^ $(VDB_VIEW_LIB)

view: test-view
	$(TEST_BINDIR)/test-view

#-------------------------------------------------------------------------------
# test-tablecursor
#
TEST_TABLECURSOR_SRC = \
	test-tablecursor

TEST_TABLECURSOR_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_TABLECURSOR_SRC))

$(TEST_BINDIR)/test-tablecursor: $(TEST_TABLECURSOR_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

tcur: test-tablecursor
	$(TEST_BINDIR)/$^

#-------------------------------------------------------------------------------
# test-wtablecursor
#
TEST_WTABLECURSOR_SRC = \
	test-wtablecursor

TEST_WTABLECURSOR_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_WTABLECURSOR_SRC))

$(TEST_BINDIR)/test-wtablecursor: $(TEST_WTABLECURSOR_OBJ)
	$(LP) --exe -o $@ $^ $(WRITE_LIBS)

twcur: test-wtablecursor
	$(TEST_BINDIR)/$^

#-------------------------------------------------------------------------------
# test-view-cursor
#
TEST_VIEW_CURSOR_SRC = \
	test-view-cursor

TEST_VIEW_CURSOR_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_VIEW_CURSOR_SRC))

$(TEST_BINDIR)/test-view-cursor: $(TEST_VIEW_CURSOR_OBJ)
	$(LP) --exe -o $@ $^ $(VDB_VIEW_LIB) -ssraxf

vcur: test-view-cursor
	$(TEST_BINDIR)/$^

#-------------------------------------------------------------------------------
# VDB-3305
#
VDB_3305_SRC = \
	test-VDB-3305

VDB_3305_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_3305_SRC))

$(TEST_BINDIR)/test-VDB-3305: $(VDB_3305_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

VDB-3305: test-VDB-3305
	$(TEST_BINDIR)/test-VDB-3305


#-------------------------------------------------------------------------------
# VDB-3418
#
VDB_3418_SRC = \
	test-VDB-3418

VDB_3418_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_3418_SRC))

$(TEST_BINDIR)/test-VDB-3418: $(VDB_3418_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

VDB-3418: test-VDB-3418
	$(TEST_BINDIR)/test-VDB-3418

#-------------------------------------------------------------------------------
# VDB-4156
#
TEST_VDB4156_SRC = \
	test-VDB-4156

TEST_VDB4156_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_VDB4156_SRC))

$(TEST_BINDIR)/test-VDB-4156: $(TEST_VDB4156_OBJ)
	$(LP) --exe -o $@ $^ $(WRITE_LIBS)

VDB-4156: test-VDB-4156
	$(TEST_BINDIR)/test-VDB-4156

#-------------------------------------------------------------------------------
# test-vdb-slow
#
TEST_VDB_SLOW_SRC = \
	test-vdb-slow

TEST_VDB_SLOW_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_VDB_SLOW_SRC))

$(TEST_BINDIR)/test-vdb-slow: $(TEST_VDB_SLOW_OBJ)
	$(LP) --exe -o $@ $^ $(READ_LIBS)

vdb-slow: test-vdb-slow
	$(TEST_BINDIR)/test-vdb-slow

#-------------------------------------------------------------------------------
# test-wvdb-slow
#
TEST_WVDB_SLOW_SRC = \
	test-wvdb-slow

TEST_WVDB_SLOW_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_WVDB_SLOW_SRC))

$(TEST_BINDIR)/test-wvdb-slow: $(TEST_WVDB_SLOW_OBJ)
	$(LP) --exe -o $@ $^ $(WRITE_LIBS)

wvdb-slow: test-wvdb-slow
	$(TEST_BINDIR)/test-wvdb-slow


