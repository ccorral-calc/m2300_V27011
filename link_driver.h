//**************************************************************
//	Define some constants used by LINK_DRIVER
//**************************************************************

typedef unsigned int	quadlet_t;
typedef unsigned short	word_t;
typedef char		byte_t;

#define MAX_PHY_LOOP		99	// number of phy reg read loops
#define LINK_LOOP_COUNT	99	// number of link reg read loops

//**************************************************************
//	Define the offsets to the OHCI registers in the TI Link IC
//**************************************************************

/* Offsets relative to context bases defined below */

#define OHCI1394_ContextControlSet            0x000
#define OHCI1394_ContextControlClear          0x004
#define OHCI1394_ContextCommandPtr            0x00C

/* register map */
#define OHCI1394_Version                      0x000
#define OHCI1394_GUID_ROM                     0x004
#define OHCI1394_ATRetries                    0x008
#define OHCI1394_CSRData                      0x00C
#define OHCI1394_CSRCompareData               0x010
#define OHCI1394_CSRControl                   0x014
#define OHCI1394_ConfigROMhdr                 0x018
#define OHCI1394_BusID                        0x01C
#define OHCI1394_BusOptions                   0x020
#define OHCI1394_GUIDHi                       0x024
#define OHCI1394_GUIDLo                       0x028
#define OHCI1394_ConfigROMmap                 0x034
#define OHCI1394_PostedWriteAddressLo         0x038
#define OHCI1394_PostedWriteAddressHi         0x03C
#define OHCI1394_VendorID                     0x040
#define OHCI1394_HCControlSet                 0x050
#define OHCI1394_HCControlClear               0x054
#define OHCI1394_HCControl_noByteSwap		0x40000000
#define OHCI1394_HCControl_programPhyEnable	0x00800000
#define OHCI1394_HCControl_aPhyEnhanceEnable	0x00400000
#define OHCI1394_HCControl_LPS			0x00080000
#define OHCI1394_HCControl_postedWriteEnable	0x00040000
#define OHCI1394_HCControl_linkEnable		0x00020000
#define OHCI1394_HCControl_softReset		0x00010000
#define OHCI1394_SelfIDBuffer                 0x064
#define OHCI1394_SelfIDCount                  0x068
#define OHCI1394_IRMultiChanMaskHiSet         0x070
#define OHCI1394_IRMultiChanMaskHiClear       0x074
#define OHCI1394_IRMultiChanMaskLoSet         0x078
#define OHCI1394_IRMultiChanMaskLoClear       0x07C
#define OHCI1394_IntEventSet                  0x080
#define OHCI1394_IntEventClear                0x084
#define OHCI1394_IntMaskSet                   0x088
#define OHCI1394_IntMaskClear                 0x08C
#define OHCI1394_IsoXmitIntEventSet           0x090
#define OHCI1394_IsoXmitIntEventClear         0x094
#define OHCI1394_IsoXmitIntMaskSet            0x098
#define OHCI1394_IsoXmitIntMaskClear          0x09C
#define OHCI1394_IsoRecvIntEventSet           0x0A0
#define OHCI1394_IsoRecvIntEventClear         0x0A4
#define OHCI1394_IsoRecvIntMaskSet            0x0A8
#define OHCI1394_IsoRecvIntMaskClear          0x0AC
#define OHCI1394_InitialBandwidthAvailable    0x0B0
#define OHCI1394_InitialChannelsAvailableHi   0x0B4
#define OHCI1394_InitialChannelsAvailableLo   0x0B8
#define OHCI1394_FairnessControl              0x0DC
#define OHCI1394_LinkControlSet               0x0E0
#define OHCI1394_LinkControlClear             0x0E4
#define OHCI1394_LinkControl_RcvSelfID		0x00000200
#define OHCI1394_LinkControl_RcvPhyPkt		0x00000400
#define OHCI1394_LinkControl_CycleTimerEnable	0x00100000
#define OHCI1394_LinkControl_CycleMaster	0x00200000
#define OHCI1394_LinkControl_CycleSource	0x00400000
#define OHCI1394_NodeID                       0x0E8
#define OHCI1394_PhyControl                   0x0EC
#define OHCI1394_IsochronousCycleTimer        0x0F0
#define OHCI1394_AsReqFilterHiSet             0x100
#define OHCI1394_AsReqFilterHiClear           0x104
#define OHCI1394_AsReqFilterLoSet             0x108
#define OHCI1394_AsReqFilterLoClear           0x10C
#define OHCI1394_PhyReqFilterHiSet            0x110
#define OHCI1394_PhyReqFilterHiClear          0x114
#define OHCI1394_PhyReqFilterLoSet            0x118
#define OHCI1394_PhyReqFilterLoClear          0x11C
#define OHCI1394_PhyUpperBound                0x120

#define OHCI1394_AsReqTrContextBase           0x180
#define OHCI1394_AsReqTrContextControlSet     0x180
#define OHCI1394_AsReqTrContextControlClear   0x184
#define OHCI1394_AsReqTrCommandPtr            0x18C

#define OHCI1394_AsRspTrContextBase           0x1A0
#define OHCI1394_AsRspTrContextControlSet     0x1A0
#define OHCI1394_AsRspTrContextControlClear   0x1A4
#define OHCI1394_AsRspTrCommandPtr            0x1AC

#define OHCI1394_AsReqRcvContextBase          0x1C0
#define OHCI1394_AsReqRcvContextControlSet    0x1C0
#define OHCI1394_AsReqRcvContextControlClear  0x1C4
#define OHCI1394_AsReqRcvCommandPtr           0x1CC

#define OHCI1394_AsRspRcvContextBase          0x1E0
#define OHCI1394_AsRspRcvContextControlSet    0x1E0
#define OHCI1394_AsRspRcvContextControlClear  0x1E4
#define OHCI1394_AsRspRcvCommandPtr           0x1EC


struct	ohci_link_regs_t {
    quadlet_t	Version;			/* 0x000 */
    quadlet_t	GUID_ROM;			/* 0x004 */
    quadlet_t	AT_Retries;			/* 0x008 */
    quadlet_t	CSR_Read_Data;			/* 0x00C */
    quadlet_t	CSR_Compare_Data;		/* 0x010 */
    quadlet_t	CSR_Control;			/* 0x014 */
    quadlet_t	Config_ROM_header;		/* 0x018 */
    quadlet_t	Bus_ID;				/* 0x01C */
    quadlet_t	Bus_Options;			/* 0x020 */
    quadlet_t	GUID_Hi;			/* 0x024 */
    quadlet_t	GUID_Lo;			/* 0x028 */
    quadlet_t	rsrv1[2];			/* 0x02C - 0x033 */
    quadlet_t	Config_ROM_Map;			/* 0x034 */
    quadlet_t	Posted_Write_Addr_Lo;		/* 0x038 */
    quadlet_t	Posted_Write_Addr_Hi;		/* 0x03C */
    quadlet_t	Vendor_ID;			/* 0x040 */
    quadlet_t	rsrv2[3];			/* 0x044 - 0x04F */
    quadlet_t	HC_Control_Set;			/* 0x050 */
    quadlet_t	HC_Control_Clear;		/* 0x054 */
    quadlet_t	rsrv3[3];			/* 0x058 - 0x063 */
    quadlet_t	Self_ID_Buffer;			/* 0x064 */
    quadlet_t	Self_ID_Count;			/* 0x068 */
    quadlet_t	rsrv4;				/* 0x06C - 0x06F */
    quadlet_t	IR_Multi_Chan_Mask_Hi_Se;	/* 0x070 */
    quadlet_t	IR_Multi_Chan_Mask_Hi_Clear;	/* 0x074 */
    quadlet_t	IR_Multi_Chan_Mask_Lo_Set;	/* 0x078 */
    quadlet_t	IR_Multi_Chan_Mask_Lo_Clear;	/* 0x07C */
    quadlet_t	Int_Event_Set;			/* 0x080 */
    quadlet_t	Int_Event_Clear;		/* 0x084 */
    quadlet_t	Int_Mask_Set;			/* 0x088 */
    quadlet_t	Int_Mask_Clear;			/* 0x08C */
    quadlet_t	ISO_Xmit_Event_Set;		/* 0x090 */
    quadlet_t	ISO_Xmit_Event_Clear;		/* 0x094 */
    quadlet_t	ISO_Xmit_Mask_Set;		/* 0x098 */
    quadlet_t	ISO_Xmit_Mask_Clear;		/* 0x09C */
    quadlet_t	ISO_Recv_Event_Set;		/* 0x0A0 */
    quadlet_t	ISO_Recv_Event_Clear;		/* 0x0A4 */
    quadlet_t	ISO_Recv_Mask_Set;		/* 0x0A8 */
    quadlet_t	ISO_Recv_Mask_Clear;		/* 0x0AC */
    quadlet_t	Initial_Bandwidth;		/* 0x0B0 */
    quadlet_t	Initial_Channels_Hi;		/* 0x0B4 */
    quadlet_t	Initial_Channels_Lo;		/* 0x0B8 */
    quadlet_t	rsrv5[8];			/* 0x0BC - 0x0DB */
    quadlet_t	Fairness_Control;		/* 0x0DC */
    quadlet_t	Link_Control_Set;		/* 0x0E0 */
    quadlet_t	Link_Control_Clear;		/* 0x0E4 */
    quadlet_t	Node_ID;			/* 0x0E8 */
    quadlet_t	PHY_Control;			/* 0x0EC */
    quadlet_t	ISO_Cycle_Timer;		/* 0x0F0 */
    quadlet_t	rsrv6[3];			/* 0x0F4 - 0x0FF */
    quadlet_t	AR_Filter_Hi_Set;		/* 0x100 */
    quadlet_t	AR_Filter_Hi_Clear;		/* 0x104 */
    quadlet_t	AR_Filter_Lo_Set;		/* 0x108 */
    quadlet_t	AR_Filter_Lo_Clear;		/* 0x10C */
    quadlet_t	PR_Filter_Hi_Set;		/* 0x110 */
    quadlet_t	PR_Filter_Hi_Clear;		/* 0x114 */
    quadlet_t	PR_Filter_Lo_Set;		/* 0x118 */
    quadlet_t	PR_Filter_Lo_Clear;		/* 0x11C */
    quadlet_t	Physical_Upper_Bound;		/* 0x120 */
    quadlet_t	rsrv7[23];			/* 0x124 - 0x17F */
    quadlet_t	AT_Req_Cntx_Ctrl_Set;		/* 0x180 */
    quadlet_t	AT_Req_Cntx_Ctrl_Clear;		/* 0x184 */
    quadlet_t	rsrv8;				/* 0x188 - 0x18B */
    quadlet_t	AT_Req_Command_Ptr;		/* 0x18C */
    quadlet_t	rsrv9[4];			/* 0x190 - 0x19C */
    quadlet_t	AT_Resp_Cntx_Ctrl_Set;		/* 0x1A0 */
    quadlet_t	AT_Resp_Cntx_Ctrl_Clear;	/* 0x1A4 */
    quadlet_t	rsrv10;				/* 0x1A8 - 0x1AB */
    quadlet_t	AT_Resp_Command_Ptr;		/* 0x1AC */
    quadlet_t	rsrv11[4];			/* 0x1B0 - 0x1BF */
    quadlet_t	AR_Req_Cntx_Ctrl_Set;		/* 0x1C0 */
    quadlet_t	AR_Req_Cntx_Ctrl_Clear;		/* 0x1C4 */
    quadlet_t	rsrv12;				/* 0x1C8 - 0x1CB */
    quadlet_t	AR_Req_Command_Ptr;		/* 0x1CC */
    quadlet_t	rsrv13[4];			/* 0x1D0 - 0x1DF */
    quadlet_t	AR_Resp_Cntx_Ctrl_Set;		/* 0x1E0 */
    quadlet_t	AR_Resp_Cntx_Ctrl_Clear;	/* 0x1E4 */
    quadlet_t	rsrv14;				/* 0x1E8 - 0x1EB */
    quadlet_t	AR_Resp_Command_Ptr;		/* 0x1EC */
    quadlet_t	rsrv15[4];			/* 0x1F0 - 0x1FF */
    quadlet_t	ISO_Xmit_0_Cntx_Ctrl_Set;	/* 0x200 */	/* Xmit Cntx 1-31 = Xmit Cntx 0 + 16*n, n=1..31 */
    quadlet_t	ISO_Xmit_0_Cntx_Ctrl_Clear;	/* 0x204 */
    quadlet_t	rsrv16;				/* 0x208 - 0x20B */
    quadlet_t	ISO_Xmit_0_Command_Ptr;		/* 0x20C */
    quadlet_t	rsrv17;				/* 0x210 - 0x3FF */
    quadlet_t	ISO_Recv_0_Cntx_Ctrl_Set;	/* 0x400 */	/* Recv Cntx 1-31 = Recv Cntx 0 + 32*n, n=1..31 */
    quadlet_t	ISO_Recv_0_Cntx_Ctrl_Clear;	/* 0x404 */
    quadlet_t	rsrv18;				/* 0x408 - 0x40B */
    quadlet_t	ISO_Recv_0_Command_Ptr;		/* 0x40C */
    quadlet_t	ISO_Recv_0_Cntx_Match;		/* 0x410 */
};


//**************************************************************
//	Define the offsets to DPR
//**************************************************************
#define DPR_BASE_ADDRESS	0x00040000

#define DPR_SELFID_BUF          0x0      // 64 bytes
#define DPR_STATUS_FIFO         0x40     // 8
#define DPR_RESERVED_1          0x48     // 8
#define DPR_LOGIN_RESP_BUF      0x50     // 16
#define DPR_AT_LOGIN_ORB        0x60     // 32
#define DPR_AT_SCSI_ORB         0x80     // 32
#define DPR_AT_PHY_CONFIG       0xA0     // 32
#define DPR_RESERVED_2          0xC0     // 64
#define DPR_AT_REQ_CTX_A        0x100    // 64
#define DPR_AT_REQ_CTX_B        0x140    // 64
#define DPR_AT_RESP_CTX_A       0x180    // 64
#define DPR_AT_RESP_CTX_B       0x1C0    // 64
#define DPR_AR_REQ_CTX_A        0x200    // 16
#define DPR_AR_REQ_CTX_B        0x210    // 16
#define DPR_AR_RESP_CTX_A       0x220    // 16
#define DPR_AR_RESP_CTX_B       0x230    // 16
#define DPR_RESERVED_3          0x240    // 32
#define DPR_CONFIG_ROM_BUF      0x400    // 512
#define DPR_HOST_BUFFER         0x600    // 512


#define DRP_RESERVED_SPACE_A   (0x80000000 + (DPR_BASE_ADDRESS + DPR_AT_REQ_CTX_A + 0x30))
#define DRP_RESERVED_SPACE_B   (0x80000000 + (DPR_BASE_ADDRESS + DPR_AT_REQ_CTX_B + 0x30))
#define DRP_RESERVED_SPACE2  (0x80000000 + (DPR_BASE_ADDRESS + DPR_RESERVED_3 + 16))

//**************************************************************
//*	 Define some data structures we will use in Stratix RAM
//**************************************************************


typedef enum
{
  context_a = 0,
  context_b
}CONTEXT;

struct ohci_more_immed_t {
  //volatile word_t	desc_reqCount;	// = 16;
    volatile quadlet_t	desc_cmd_word;	// = 0x0200;
    volatile quadlet_t	reserved[3];	// = 0;
    volatile quadlet_t	payload[4];		// = IEEE Packet Header
  };

struct ohci_last_immed_t {
  //volatile word_t	desc_reqCount;	// = 16;
  volatile quadlet_t	desc_cmd_word;	// = 0x120C;
  volatile quadlet_t	reserved;		// = 0;
  volatile quadlet_t	branch_address;	// = 0;
  //volatile word_t	xferStatus;		// = 0;
  volatile quadlet_t	timeStamp;		// = 0;
  volatile quadlet_t	payload[4];		// = 64-bit ORB address, 64-bit pad=0
};


struct ohci_last_t {
  //volatile word_t       desc_reqCount;   // = 16
  volatile quadlet_t       desc_cmd_word;   // = 0x100C
  volatile quadlet_t    data_address;
  volatile quadlet_t    branch_address;  // = 0
  //volatile word_t       xferStatus;      // = 0
  volatile quadlet_t       timeStamp;       // = 0
};

struct ohci_command_block_t {
  struct ohci_more_immed_t ohci_more_immed;
  struct ohci_last_t ohci_last;
  volatile quadlet_t resv[4];
};

struct ohci_input_more_t {
  //volatile word_t	reqCount;		// = input buffer size in bytes
  volatile quadlet_t	desc_cmd_word;	// = 0x280C
  volatile quadlet_t	dataAddress;	// = longword aligned buffer address
  volatile quadlet_t	branchAddress;	// = pointer to next descriptor
  volatile quadlet_t	resCount;		// = number of unused buffer bytes
  //volatile word_t	xferStatus;		// = copy of ContextCtrl[15:0]
};

struct scsi_cdb_t {
  volatile quadlet_t  cdb_blk[3];
};

struct sbp2_scsi_orb_t {
  volatile quadlet_t	next_orb_mslw;	// = 0x80000000;
  volatile quadlet_t	next_orb_lslw;	// = 0;
  volatile quadlet_t	data_desc_mslw;	// = 0;
  volatile quadlet_t	data_desc_lslw;	// = 0;
  volatile quadlet_t dir_size;       // direction, speed and data size
  volatile struct scsi_cdb_t	cdb;
};

struct sbp2_login_orb_t {
  volatile quadlet_t	password_ptr[2];	// = 0
  volatile quadlet_t	login_resp_ptr[2];	// addr of DPR login_response buffer;
  // volatile word_t	lun;				// = 0
  //volatile word_t	cmd_word;			// = 0x9000
  //volatile word_t	login_resp_length;	// = 16;
  //volatile word_t	password_length;	// = 0
  volatile quadlet_t cmd_word;
  volatile quadlet_t login_resp_length;
  volatile quadlet_t	status_fifo_ptr[2];	// addr pf STATUS_FIFO_status
};


//***************************************************************
//*	          Define the offsets to the Dual Port RAM
//***************************************************************




  //#pragma pack(1)


#if 0 //old def. bdu100206
struct dual_port_RAM_t {
    volatile quadlet_t		SELF_ID_BUF[512];
    volatile quadlet_t		CONFIG_BUF[512];
    struct ohci_command_block_t	AT_REQ_CTX_A;
    struct ohci_command_block_t	AT_REQ_CTX_B;
    struct ohci_command_block_t	AT_RSP_CTX_A;
    struct ohci_command_block_t	AT_RSP_CTX_B;
    struct ohci_input_more_t	AR_REQ_CTX_A;
    struct ohci_input_more_t	AR_REQ_CTX_B;
    struct ohci_input_more_t	AR_RSP_CTX_A;
    struct ohci_input_more_t	AR_RSP_CTX_B;
    struct ohci_last_immed_t	AT_PHY_CFG;
    volatile quadlet_t		reserved_1[40];
    struct sbp2_scsi_orb_t	AT_SCSI_ORB;
    struct sbp2_login_orb_t	AT_LOGIN_ORB;
    volatile quadlet_t		LOGIN_RSP_BUF[4];
    volatile quadlet_t		STATUS_FIFO[2];
    volatile quadlet_t		reserved_2[874];
    volatile quadlet_t		AT_REQ_BUF[512];
    volatile quadlet_t		AT_RSP_BUF[512];
    volatile quadlet_t		AR_REQ_BUF[512];
    volatile quadlet_t		AR_RSP_BUF[512];
    volatile quadlet_t		XMIT_FIFO[1024];
    volatile quadlet_t		RECV_FIFO[1024];
};
#endif

struct dual_port_RAM_t {
  volatile quadlet_t		SELF_ID_BUF[16];
  volatile quadlet_t		STATUS_FIFO[2];
  volatile quadlet_t		reserved[2];
  volatile quadlet_t		LOGIN_RSP_BUF[4];
  struct sbp2_login_orb_t	AT_LOGIN_ORB;
  struct sbp2_scsi_orb_t	AT_SCSI_ORB;
  struct ohci_last_immed_t	AT_PHY_CFG;
  volatile quadlet_t		reserved2[16];
  struct ohci_command_block_t	AT_REQ_CTX_A;
  struct ohci_command_block_t	AT_REQ_CTX_B;
  struct ohci_command_block_t	AT_RSP_CTX_A;
  struct ohci_command_block_t	AT_RSP_CTX_B;
  struct ohci_input_more_t	AR_REQ_CTX_A;
  struct ohci_input_more_t	AR_REQ_CTX_B;
  struct ohci_input_more_t	AR_RSP_CTX_A;
  struct ohci_input_more_t	AR_RSP_CTX_B;
  volatile quadlet_t		reserved3[8];
  volatile quadlet_t		CONFIG_BUF[128];
  volatile quadlet_t		HOST_BUF[128];
};

//#pragma pack(0)
