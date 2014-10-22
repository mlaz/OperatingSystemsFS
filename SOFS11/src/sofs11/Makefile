CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO11"

OBJS  = sofs_blockviews.o sofs_basicoper.o
OBJS += sofs_ifuncs_1_ai.o sofs_ifuncs_1_fi.o
OBJS += sofs_ifuncs_1_adc.o sofs_ifuncs_1_fdc.o
OBJS += sofs_ifuncs_2_ri.o sofs_ifuncs_2_wi.o
OBJS += sofs_ifuncs_2_ci.o sofs_ifuncs_2_ag.o
OBJS += sofs_ifuncs_3_rfc.o sofs_ifuncs_3_wfc.o
OBJS += sofs_ifuncs_3_hfc.o sofs_ifuncs_3_hfcs.o sofs_ifuncs_3_cdc.o
OBJS += sofs_ifuncs_4_gdebp.o sofs_ifuncs_4_gdebn.o
OBJS += sofs_ifuncs_4_ade.o sofs_ifuncs_4_rmde.o sofs_ifuncs_4_rnde.o
OBJS += sofs_ifuncs_4_cde.o sofs_ifuncs_4_att.o sofs_ifuncs_4_det.o

GIVEN_OBJS  = sofs_basicconsist.o
GIVEN_OBJS += sofs_ifuncs_1_bin.o
GIVEN_OBJS += sofs_ifuncs_2_bin.o
GIVEN_OBJS += sofs_ifuncs_3_bin.o
GIVEN_OBJS += sofs_ifuncs_4_bin.o

all:			$(GIVEN_OBJS) libsofs11

libsofs11:	$(OBJS)
			ar -r libsofs11.a $(GIVEN_OBJS) $^
			cp libsofs11.a ../../lib
			rm -f $^ libsofs11.a

clean:
			rm -f ../../lib/libsofs11.a
