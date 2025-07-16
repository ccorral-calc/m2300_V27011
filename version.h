//////////////////////////////////////////////////////////////////////////////
//
//    Copyright � 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: version.h
//    Version: 1.0
//     Author: pc
//
//            MONSSTR 2300 V1,V2
//
//            Version Header File
//
//    Revisions:   
//         10-26-2006 11:57pm - Added support for Stopping Playback
//         10-27-2006 12:05pm - Add a call to do_loop_test for 50ft remote
//         10-27-2006 1:11pm  - Removed Printfs
//         10-27-2006 3:13pm  - Fixed Updating Stanag during record
//         10-28-2006 11:37am - Changed 720X480 to 718x478, Removed CCIR reset go
//         10-28-2006 12:51pm - Added 0x1E to 5..0, Added 6 11..8 in channel + 104
//         10-28-2006 03:52pm - Added M1553 Events and also Added Reset Unit code
//         10-28-2006 11:05pm - Changed ABS Upper and Lower in M1553 Event
//                              Added Stop RECORD and Stop Play
//         10-28-2006 11:52pm - Fixed Stop play Stop Record and Removed prints
//         10-29-2006 1:17pm -  Fixed Trick Modes, Added a Pause bit for Video
//         10-29-2006 3:13pm -  Added Delays in M1553 Command Processing
//         10-30-2006 10:32pm -  Fixed Pause and Resume Video
//         10-31-2006 11:10am -  Removed Reset From Setup. Added old Overlay parameters
//                               for config == 1.
//         11-02-2006 8:45am -  Fixes from ATP,Added special code for Auto Record
//                              Changed Bus Message Filtering. Changed M1553 Events
//         11-03-2006 2:30pm -  Added Event Toggle. 
//         11-06-2006 4:20pm -  Changed the way Day and Time are set in version and TMATS
//         11-07-2006 3:03pm -  added p0 =1 ,p1 =3, p2 =3 as a 1394B controller and as a Remote
//         11-07-2006 3:46pm -  added 2 second wait after force_root_reset
//         11-08-2006 7:30pm -  More robust check for cartridge login. Check if cartridge is connected
//                              in order to go into record with ssric and discretes
//         11-09-2006 3:05pm -  Changed Message Filtering to Channel of Type
//         11-09-2006 4:16pm -  Clear Voice Enable when changing setups
//         11-14-2006 2:16pm -  Put back retrieving Node Packets to be used in Playback
//         11-15-2006 6:22pm -  Added a Flush command to the cartridge to fix bad data during shutdown
//         11-16-2006 11:25am -  Fixed 32-bit block stanag updates. Was causing a problem past 4GB
//         11-16-2006 11:29am -  Fixed RTEST test number
//         11-16-2006 17:09pm -  Changed DiskWrite to wait for Host Complete
//         11-21-2006 17:38pm -  Added Ethernet Channel Support
//         11-27-2006 15:42pm -  Changed wait time for 1394B force_root command
//         11-29-2006 14:05pm -  Fixed Event Overlay Toggle
//         11-29-2006 16:22pm -  Fixed Removal of Cartridge During Record. Remove Close Time from .Files
//                               command for config 0. Added a check for Cartridge Full during mount. If 
//                               Full turn on Fault LED
//         11-29-2006 17:49pm -  Increased Sleep for auto mounting 106GB cartridge
//         11-29-2006 17:58pm -  Change Time Channel to not report No Signal if not External
//         12-01-2006 15:15pm -  Added More Ethernet Support
//         12-02-2006 17:05pm - Added TMATS attributes for MAC Address and Format
//         12-04-2006 13:19pm - Added back the setting of the RT via the .set command,and only start
//                              the M1553 command thread when in config 0
//
//         12-04-2006 15:41pm - Changed Register 0x14 to 0x11(Enable Loopback), was 0x1
//         12-05-2006 11:00am - Fixed Updating the STANAG. Added Debug to setting the RT
//         12-05-2006 12:02am - Changed RT to use the CORE addressing not CSR addressing
//         12-05-2006 17:13pm - Added a delay for Auto-Record Config ->2
//         12-06-2006 15:44pm - Changed PHY programming. Used Linux as template
//         12-06-2006 15:33pm - Remove Auto Negotiate for Airbus
//         12-07-2006 21:42pm - Search for Ethernet Address
//         12-08-2006 10:51pm - Hardware Reset for Ethernet
//         12-12-2006 16:08pm - Changed the way dismount/mount works with unit in the system and not removed
//         12-13-2006 17:15pm - Changed the amount to save at the end of the cartridge to 16MB
//         01-05-2007 16:03pm - Added support for Airbus Discretes, config == 3
//         01-10-2007 10:45am - Added support for Edwards AFB, config == 4
//         01-25-2007 10:10am - Removed Declass writing to file, Added Debug to M1553 Comand and Control
//         01-25-2007 10:10am - Changed to Version 8.0
//         01-28-2007 13:59pm - Add Date From TMATS, Cleaned up debug for V2 ATOP
//         01-29-2007 15:28pm - Changed Pause for M1553 and Analog. Added Local Go when removing Enable
//         01-29-2007 17:36pm - Remove Reset from Upgrade from RMM
//         01-30-2007 10:51am - Fixed M1553 Events again!!!
//         02-07-2007 16:49pm - Fixed Ethernet Auto-Negotiation,wait for it to complete before
//                              Sending Data
//         03-05-2007 13:53pm - Added support for both Two and Four Audio Chip Video Boards
//         03-06-2007 17:13pm - Get Two and Four Audio Chip Version from Video Boards
//         03-07-2007 16:24pm - Added config == 4 to Record Discrete
//         03-15-2007 14:07pm - Add Lockheed Discretes. Support both versions of Overlay TMATS attributes
//         04-06-2007 14:13pm - New Indexes and Events
//         04-08-2007 13:03pm - Added new M1553 Events
//         04-10-2007 10:28am - Add NumberOfBlocksWritten to new Stanag Update
//         04-11-2007 11:42am - Make M1553 Events 0 Address based
//         04-11-2007 15:56pm - Tmats length should be mod 4 + 2
//         04-12-2007 12:13pm - Only one node entry for length and M1553 Time fixes
//         04-13-2007 07:40pm - Changed TRA,STA,DWC from B group to binary
//         04-17-2007 11:53am - Changed Jam time to always 2 seconds
//         04-24-2007 12:13pm - Fixed Converting Time with Milli seconds
//         04-27-2007 12:42pm - Added Global Audio Map on Controller
//         04-30-2007 14:25pm - Fixed the updating of the Root Position. 
//         05-02-2007 10:11am - Changed Audio I2C settings
//         05-04-2007 14:51pm - Changed the RT Control Settings
//         05-10-2007 10:55am - Changed the TMATS buffer to 1Meg
//         05-18-2007 11:53am - Changed the .Play to give invalid mode when in play or rec-play. Added ..speed
//         05-18-2007 14:00pm - Made Changes to comply with TRD Rev D
//         05-18-2007 17:36pm - Added RI5 for date
//         05-20-2007 20:27pm - Added LiveVid(0) when stopping playback and Voice 1 is Right and Voice 2 Left
//         05-22-2007 10:27am - Stanag Changes to Support 106-7
//         05-22-2007 12:35pm - Added UART health bits
//         05-22-2007 17:52pm - Add LiveVideo from TMATS
//         05-23-2007 09:06pm - Removed Echo from COM. Only Lockheed has echo
//         05-24-2007 08:13am - Changed RGB Setup to original Values
//         05-24-2007 14:40pm - Added Code to determine if Bit Sync is locked
//         05-30-2007 16:56pm - Fixed Filtering and 1553 STATUS Response
//         05-31-2007 18:14pm - TRD Fixes,make RO event command word only and T9 use word 22
//         06-02-2007 17:53pm - New TMATS Reading and Setup from Cartridge
//         06-03-2007 21:49pm - Fixed recording file when erasing,only when tmats loaded from RMM
//         06-04-2007 18:11pm - Added One Event Per packet
//         06-04-2007 21:03pm - Added support for Event Roll over recording
//         06-05-2007 21:37pm - Fixes found during ATP 1
//         06-06-2007 13:59pm - Added New Pause Resume Setup for 1553
//         06-07-2007 14:15pm - New Time Algorithm, Increment Number of Events for 1553 Events.
//         06-11-2007 13:38pm - Only allow a max of 20 events per packet, Do not send .erase to RMM to erase Tmats
//         06-12-2007 11:43am - Release 
//         06-18-2007 09:42am - Changed TimeCodeOffset to effectively always be 50,000,000. (M23_Controller.c)
//                            - Changed part of the F15 EGI Time sync system.
//         06-27-2007 10:39am - New Filtering and New TMATS saving-security from RMM
//         06-29-2007 11:10am - change the working of a two audio board
//         07-03-2007 15:17pm - Fixed Going into record with discrete when mounting and power up
//         07-05-2007 13:07pm - Release for RMM wizard and Discrete Record and Setting Up Filtering for Errors
//         07-09-2007 13:58pm - Fixed the logging of Root Packets. Use FIFO empty instead of Root Count
//         07-10-2007 18:05pm - Per Al's request, added a 250ms debounce on the Record Discrete
//         07-11-2007 17:33pm - Per Al's request, Record must be on until one root has been recorded, Sync time From 
//                              RMM at Power up fixed
//         07-21-2007 11:46am - New TRD Test with new RT Command And Control
//         07-22-2007 12:00am - Changes SA1 and Health Mask to 8-bits
//         07-25-2007 06:00pm - ATP fixes and Release
//         07-27-2007 13:00pm - Fixed Loading TMATS with RMM Wizard. Start Recording at file001 when config
//                              file is present
//         07-31-2007 13:00pm - Changed to support new Video Board Logic. Let Logic Poll I2C for status
//         08-07-2007 13:36pm - Added Lockheed to the Configuration, Initial Release
//         08-20-2007 17:00pm - Added Setting up Video Boards I2c if no setup is present
//         08-24-2007 17:00pm - Changed PCM setup for 12 and 24 bits
//         08-29-2007 13:30pm - Added code for accuray for a .time RT command
//         09-04-2007 13:30pm - RT .Status response fixes
//         09-05-2007 14:30pm - Added Ptest functionality
//         09-07-2007 15:00pm - Added support for Page Tables.
//         09-11-2007 16:00pm - Fixed Number of Minors to use old way
//         09-12-2007 10:00am - Added Events for Bus Pause Resume and Time Sync
//         09-13-2007 14:30pm - changed 3x to 2x
//         09-14-2007 12:00pm - Added PCM Calculation for SFID
//         09-14-2007 16:20pm - Put 32 in 0x108 if only 1 minor per major
//         09-17-2007 12:00pm - Release V1.007
//         09-18-2007 11:00pm - Setting secondary time from RMM during Mount
//         09-24-2007 16:30pm - Added Channel Paused,Video Not Making Packets Events`
//         09-25-2007 15:00pm - Clear Bingo Health with transition to Memory Full
//         09-27-2007 16:00pm - Added Data LED for Lockheed, Release 2.009
//         10-01-2007 10:00pm - Removed LED Blink earlier than usual for Lockheed
//         10-01-2007 15:00pm - Added Audio Tone, used Bus speed instead of Remote to configure PHYs
//         10-01-2007 17:00pm - Added Lockheed event when .record with filename
//         10-01-2007 17:00pm - Added Lockheed event when .record with filename
//         10-02-2007 15:00pm - Added Page Tables
//         10-05-2007 16:00pm - Added Media IO Error Status
//         10-09-2007 17:00pm - Forced No Mute on Compressor
//         10-10-2007 18:00pm - Added double setup to fix Audio Mute
//         10-11-2007 11:30pm - Remove ready for lockheed during BIT
//         10-11-2007 18:00pm - Fixed Mount/Dismount LED, .chan and Erase Discrete when in record (LOCKHEED)
//         10-12-2007 12:00pm - Fixed Mount/Dismount LED, .chan and Erase Discrete when in record (LOCKHEED)
//         10-12-2007 03:45pm - Do not stop on a Critical Error During Record
//         10-16-2007 11:00am - Added Start/Stop recording with EGI time sync
//         10-17-2007 14:30pm - Do not set the M1553 sync time bit until time is synced and recording has stopped
//         10-22-2007 15:00pm - Change where we retrieve 1394 Status on Host and Record Faults
//         10-30-2007 10:00am - Only Set fault when Critical is set(always when Disk Not connected,Media Full, Config Fail.
//         10-30-2007 17:00pm - Fixed Event Switch Event on all other Configurations (other than 0)
//         11-01-2007 13:00pm - Check If Cartridge is not in correct location.If it is Dismount
//         11-02-2007 08:00am - Add PCM Pause
//         11-08-2007 14:00pm - Moved FeatureDescriptions to M23_Controller.h
//         12-14-2007 13:30pm - Only Host Busy and Host Status(0x4100) to determine if command is complete
//         12-14-2007 15:00pm - Startup SSRIC regardless of TMATS is loaded via RMM
//         01-02-2008 15:00pm - Force 1394 to S400
//         01-09-2008 16:00pm - Fixed type when adding 8 CH PCM version to tmats
//         01-10-2008 16:00pm - Added Valid Bit For 1553 Command Control Status Response. Change 3x to 2x playback.
//         01-18-2008 10:00pm - Added .Queue States and Anamoly fixes.
//         02-07-2008 12:00pm - Fixes for DVRS Anamolies list. Added Live Video to 1553 RT Replay and Status
//         02-11-2008 10:00am - Added a semaphore for Read and Write during record
//         02-20-2008 15:00am - Added a reset to the Video Output chip during Live Video playback.
//         03-04-2008 10:00am - Added B1B Config
//         03-10-2008 10:00am - Added B1B Time SYNC From EGI
//         03-13-2008 10:00am - Fixed Setting LiveVideo from RT after Bootup - Anomaly 32
//         03-13-2008 10:00am - Added Stopping Of Live Video From RT with Stop Play Command - Anomaly 33
//         03-20-2008 17:00pm - Added Pulse Counter and Controller UART TMATS Parameters
//         03-27-2008 11:00am - Added Pulse Counter Code to PCM
//         04-09-2008 10:00am - When using Pulse Counter on PCM, set to 422
//         04-11-2008 11:00am - Added wait for Global Busy to clear during Record, Changed 1553 Max packet size from 1950 to 1300
//         04-11-2008 15:00pm - Moved Setting Up Ethernet before Global Go is turned on, Set Ethernet INT and ENABLE
//         04-12-2008 15:00pm - Removed Sending ARP. Added Sending TMATS once Broadcast and Global Record are set
//         04-23-2008 16:00pm - Removed Chunk done count when updating Stanag and Root Pointers
//         05-16-2008 16:00pm - Added GGA to the GPS Messages that are allowed and recorded.
//         05-21-2008 16:00pm - Do Not report Time Signal error when jamming GPS. Added Length to GPS Payload(IPH)
//         06-03-2008 14:00pm - Changed Ethernet Header to Big ENDIAN. Added IP/Ethernet Multicast
//         06-17-2008 16:00pm - Changed GPS data to be sent to Event Channel even if not is RECORD. Fixed Setting DMP broadcast
//         06-17-2008 16:00pm - Fixed Version in TMATS file
//         07-30-2008 16:00pm - Added a Video Board to GTRI
//         08-01-2008 16:00pm - Do not Publish Video
//         08-06-2008 11:00am - We are now able to stop on Auto Record.
//         03-19-2009 09:00am - Remove the Report Errors Bit Per Lukes Request 
//         03-17-2010 14:00pm - Allow Video Publish and modify RAM for Publish
//         03-24-2010 15:00pm - Fixed Year > 2009
//         06-01-2010 16:00   - Added Dynamic Mac Addresses
//         06-02-2010 06:00   - Added code for Expansion Flash Parts
//         08-04-2010 11:00   - Change read macaddress to macaddress.txt. Fixed Nulls in data
//         08-01-2011 15:00   - Changed the Destination MAC Address to all 1's
//         12-05-2011 17:00   - Change TFOM to > 10ms
//         02-06-2013 14:00   - Flush out data at end of recording
//         02-13-2013 14:00   - User RECORD_OUT_POINTER to determine EOM and change to 64MB
//         02-14-2013 11:00   - Do Not Allow NMEA to be recorded if not enabled in TMATS
//         08-19-2013 10:00   - Allow for coninuous jam for B52 which is now config 8
//         08-26-2013 08:00   - Support for two digit version and remove continuos jam as logic uses the 1PPS
//         09-05-2013 16:00   - Added a GPS Not Synced bit, will clear if at anytime we have jammed time from GPS
//         09-09-2013 08:00   - Add reallocation of Nodes for extremely long recordings
//         09-23-2013 13:00   - Need to setup Ethernet before Starting the hardware
//         12-17-2013 08:00   - Added GSV GPS Message
//         03-03-2014 18:00   - 4 channel 1553 1 Discrete board setup missing
//         07-18-2014 11:00   - Changed Serial Port to Raw
//         01-26-2015 09:00   - Adding .set script to startup processing
//         02-06-2015 17:00   - Added NMEA Debug
//         02-12-2015 13:00   - Changed the Time Jam Algorithm to the same as BUS time Jam to remove 1 second dither
//         02-13-2015 16:00   - Added Newer M1553 board
//         02-17-2015 11:00   - Fixed Event Allocation(limit is 16) and added GPS Debug
//         02-18-2015 16:00   - Fixed TMATS issues with the end containing strange characters
//         02-19-2015 14:00   - Change PCM board new Algorithm
//         02-26-2015 17:00   - Add Broadcast mask to m1553
//         02-27-2015 10:00   - Add Back in the UART on the Controller. Accidently removed it 
//         03-24-2015 09:00   - Fixed Auto Publish race condition with the GPS UART channel
//         07-21-2015 14:00   - Change UART Configuration of PCM Channels from 0x180 to 0x194
//         06-30-2016 10:00   - Add Fault and Bingo Blink
//         07-27-2016 14:00   - Add a read of the Filter bit as to not remove the Virtual Exclude
//         01-22-2018 17:00   - Merge Ethernet Publish and Record
//         06-01-2018 09:00   - Start Ethernet Publish if Ethernet Channel is disabled not only when missing
//         10-15-2018 16:00   - Hard Code Settting Time Jam to 2 seconds to match the logic
//         10-31-2018 08:00   - Added in .TMATS Delete
//         11-08-2018 15:00   - Add in Delay for 2nd CPU. Flash Collision
//         01-23-2019 14:00   - Support UART Baud Rates of 230400 and 460800




#define VERSION_DATE        "Jul 16 2025" 
#define VERSION_TIME        "14:00:00"

