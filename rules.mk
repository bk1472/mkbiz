#######################################################################
# Common Rules
#######################################################################
.PHONY: depend clean clobber distclean


clean:
	@$(RM) $(OBJS)

clobber: clean
	@$(RMDIR) $(OBJ_DIR)
	@$(RM) $(TGT)

distclean: clobber ccclean
	@$(RM) tags

$(OBJ_DIR)/%.d : %.cpp
	@$(ECHO) "+++++++++ Making $(notdir $@)"
	@$(TST) -d $(OBJ_DIR) || mkdir -p $(OBJ_DIR)
	@( $(CXX) -MM $(CXXFLAGS) $<											\
	  | sed  -f $(UTIL_DIR)/mkdep.sed										\
	  | grep -v "^  \\\\"													\
	  | sed  -e "s\$(<:.cpp=.o)\$@ $(OBJ_DIR)/$(<:.cpp=.o)\g"				\
	) > $@ 																	\
	$(NULL)

$(OBJ_DIR)/%.d : %.c
	@$(ECHO) "+++++++++ Making $(notdir $@)"
	@$(TST) -d $(OBJ_DIR) || mkdir -p $(OBJ_DIR)
	@( $(CC) -MM $(CFLAGS) $<												\
	  | sed  -f $(UTIL_DIR)/mkdep.sed										\
	  | grep -v "^  \\\\"													\
	  | sed  -e "s\$(<:.c=.o)\$@ $(OBJ_DIR)/$(<:.c=.o)\g"					\
	) > $@ 																	\
	$(NULL)

$(OBJ_DIR)/%.o:%.cpp
	@$(TST) -d $(OBJ_DIR) || mkdir -p $(OBJ_DIR)
	@$(CCDV) $(CXX) -c $(CXXFLAGS) -o $@ $<

$(OBJ_DIR)/%.o:%.c
	@$(TST) -d $(OBJ_DIR) || mkdir -p $(OBJ_DIR)
	@$(CCDV) $(CC) -c $(CFLAGS) -o $@ $<

$(TGT) : $(OBJS)
	@$(CCDV) $(LD) -o $@ $(OBJS) $(LFLAGS)
	@$(STRIP) $@

ifeq ($(OBJS),)
INCLUDE_DEPEND	?= 0
else
INCLUDE_DEPEND	?= 1
ifneq ($(MAKECMDGOALS),)
ifneq ($(MAKECMDGOALS), depend)
INCLUDE_DEPEND	 = 0
endif
endif
endif

ifeq ($(INCLUDE_DEPEND), 1)
-include $(OBJS:.o=.d)
endif

$(CCDV): $(UTIL_DIR)/ccdv.src/ccdv.c $(UTIL_DIR)/ccdv.src/sift-warn.c
ifeq ($(MAKELEVEL), 0)
	@$(MAKE) -C $(UTIL_DIR)/ccdv.src
endif

ccclean:
	@$(MAKE) -C $(UTIL_DIR)/ccdv.src clean
