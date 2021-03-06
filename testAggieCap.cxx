/**
 * This application simulates AggieCap software and sends camera and payload info,
 * as well as request for moving PAYLOAD waypoint. Eventually it will need to be synchronized
 * with the flight plan, for now we hardcode the constants.
 *
 * Binds to:
 * WP_MOVED (for updates about the waypoint position)
 * VECTORNAV_INFO (for uncertainty estimates)
 * ATTITUDE (for FW attitude)
 * ROTORCRAFT_FP (for Rotorcraft attitude)
 * GPS_LLA (for FW/ROTORCRAFT position info and time info)
 *
 * Other messages it binds to:
 * ACTUATORS
 * COMMANDS
 *
 * See https://github.com/paparazzi/pprzlink/blob/master/message_definitions/v1.0/messages.xml
 * for message definition (useful when you are parsing the messages).
 */


#include <iostream> // for io
#include <thread> // for threads
#include <chrono> // for thread sleep
#include <string> // for string handling
#include <unistd.h> // for getopt
#include <sstream> // string operations
#include <string.h> // strings
#include <vector> // for vectors

#include "Ivycpp.h"
#include "IvyApplication.h"

using namespace std;

class AggieCapTest : public IvyApplicationCallback, public IvyMessageCallback {
public:
  Ivy *bus;

  void Start();
  void OnApplicationConnected(IvyApplication *app);
  void OnApplicationDisconnected(IvyApplication *app);
  void OnApplicationCongestion(IvyApplication *app);
  void OnApplicationDecongestion(IvyApplication *app);
  void OnApplicationFifoFull(IvyApplication *app);
  void OnMessage(IvyApplication *app, int argc, const char **argv){};

  IvyMessageCallbackFunction cb_wp_moved;
  IvyMessageCallbackFunction cb_vectornav_info;
  IvyMessageCallbackFunction cb_attitude;
  IvyMessageCallbackFunction cb_gps_lla;
  IvyMessageCallbackFunction cb_rotorcraft_fp;

  static void OnWP_MOVED(IvyApplication *app, void *user_data, int argc, const char **argv);
  static void OnVECTORNAV_INFO(IvyApplication *app, void *user_data, int argc, const char **argv);
  static void OnATTITUDE(IvyApplication *app, void *user_data, int argc, const char **argv);
  static void OnGPS_LLA(IvyApplication *app, void *user_data, int argc, const char **argv);
  static void OnROTORCRAFT_FP(IvyApplication *app, void *user_data, int argc, const char **argv);

  static void periodic_camera_snapshot(AggieCapTest *test);
  static void periodic_camera_payload(AggieCapTest *test);
  static void periodic_move_wp(AggieCapTest *test);
  static void periodic_send_time(AggieCapTest *test);
  static void ivy_thread(AggieCapTest *test);

  AggieCapTest(char *domain, bool debug, char* name);
  AggieCapTest(char *domain, bool debug);
  AggieCapTest(char *domain);
  AggieCapTest();


private:
  static void ivyAppConnCb( IvyApplication *app ) {};
  static void ivyAppDiscConnCb( IvyApplication *app ) {};

  const char *bus_domain_;
  const char* WP_MOVED = "^(\\S*) WP_MOVED (\\S*) (\\S*) (\\S*) (\\S*) (\\S*)";
  const char* VECTORNAV_INFO = "^(\\S*) VECTORNAV_INFO (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*)";
  const char* ATTITUDE = "^(\\S*) ATTITUDE (\\S*) (\\S*) (\\S*)";
  const char* GPS_LLA =  "^(\\S*) GPS_LLA (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*)";
  const char* ROTORCRAFT_FP = "^(\\S*) ROTORCRAFT_FP (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*) (\\S*)";

  float sec_since_startup_;

  string name_ = "aggiecap"; // ivy node name
  bool debug_; // are we in debug mode?

  // sender message names
  std::vector<std::string> camera_snapshot_ = {"CAMERA_SNAPSHOT_DL","CAMERA_SNAPSHOT"};
  std::vector<std::string> camera_payload_ = {"CAMERA_PAYLOAD_DL","CAMERA_PAYLOAD"};
};

AggieCapTest::AggieCapTest(char *domain, bool debug, char* name)
: cb_wp_moved(OnWP_MOVED,this),
  cb_vectornav_info(OnVECTORNAV_INFO,this),
  cb_attitude(OnATTITUDE,this),
  cb_gps_lla(OnGPS_LLA,this),
  cb_rotorcraft_fp(OnROTORCRAFT_FP,this)
  {
  debug_ = debug;
  sec_since_startup_ = 0.0;
  bus_domain_= domain;
  if(name!=NULL){
    this->name_ = string(name);
  }
  bus = new Ivy(this->name_.c_str(), "AggieCapTest READY",
      BUS_APPLICATION_CALLBACK(  ivyAppConnCb, ivyAppDiscConnCb ),false);
}

AggieCapTest::AggieCapTest(char *domain, bool debug)
: cb_wp_moved(OnWP_MOVED,this),
  cb_vectornav_info(OnVECTORNAV_INFO,this),
  cb_attitude(OnATTITUDE,this),
  cb_gps_lla(OnGPS_LLA,this),
  cb_rotorcraft_fp(OnROTORCRAFT_FP,this)
  {
  debug_ = debug;
  sec_since_startup_ = 0.0;
  bus_domain_= domain;
  bus = new Ivy( "AggieCapTest", "AggieCapTest READY",
      BUS_APPLICATION_CALLBACK(  ivyAppConnCb, ivyAppDiscConnCb ),false);
}

AggieCapTest::AggieCapTest(char *domain)
: cb_wp_moved(OnWP_MOVED,this),
  cb_vectornav_info(OnVECTORNAV_INFO,this),
  cb_attitude(OnATTITUDE,this),
  cb_gps_lla(OnGPS_LLA,this),
  cb_rotorcraft_fp(OnROTORCRAFT_FP,this)
  {
  debug_ = false;
  sec_since_startup_ = 0.0;
  bus_domain_= domain;
  bus = new Ivy( "AggieCapTest", "AggieCapTest READY",
      BUS_APPLICATION_CALLBACK(  ivyAppConnCb, ivyAppDiscConnCb ),false);
}


AggieCapTest::AggieCapTest()
: cb_wp_moved(OnWP_MOVED,this),
  cb_vectornav_info(OnVECTORNAV_INFO,this),
  cb_attitude(OnATTITUDE,this),
  cb_gps_lla(OnGPS_LLA,this),
  cb_rotorcraft_fp(OnROTORCRAFT_FP,this)
  {
  debug_ = false;
  sec_since_startup_ = 0.0;
  bus_domain_= NULL;
  bus = new Ivy( "AggieCapTest", "AggieCapTest READY",
      BUS_APPLICATION_CALLBACK(  ivyAppConnCb, ivyAppDiscConnCb ),false);
}


void AggieCapTest::Start()
{
  // bind messages
  bus->BindMsg(WP_MOVED, &cb_wp_moved);
  bus->BindMsg(VECTORNAV_INFO, &cb_vectornav_info);
  bus->BindMsg(ATTITUDE, &cb_attitude);
  bus->BindMsg(GPS_LLA, &cb_gps_lla);
  bus->BindMsg(ROTORCRAFT_FP, &cb_rotorcraft_fp);

  // start bus
  bus->start(bus_domain_);

  // enter main loop
  bus->ivyMainLoop();
}


void AggieCapTest::OnWP_MOVED ( IvyApplication *app, void *user_data, int argc, const char **argv )
{
  AggieCapTest* copilot = static_cast<AggieCapTest*>(user_data);
  printf ("Got WP_MOVED message.\n");
}

void AggieCapTest::OnVECTORNAV_INFO ( IvyApplication *app, void *user_data, int argc, const char **argv )
{
  AggieCapTest* copilot = static_cast<AggieCapTest*>(user_data);
  printf ("Got OnVECTORNAV_INFO message.\n");
}

void AggieCapTest::OnATTITUDE ( IvyApplication *app, void *user_data, int argc, const char **argv )
{
  AggieCapTest* copilot = static_cast<AggieCapTest*>(user_data);
  printf ("Got OnATTITUDE message.\n");
}

void AggieCapTest::OnGPS_LLA ( IvyApplication *app, void *user_data, int argc, const char **argv )
{
  AggieCapTest* copilot = static_cast<AggieCapTest*>(user_data);
  printf ("Got GPS_LLA message.\n");
}

void AggieCapTest::OnROTORCRAFT_FP ( IvyApplication *app, void *user_data, int argc, const char **argv )
{
  AggieCapTest* copilot = static_cast<AggieCapTest*>(user_data);
  printf ("Got ROTORCRAFT_FP message.\n");
}


void AggieCapTest :: OnApplicationConnected (IvyApplication *app)
{
  const char *appname;
  const char *host;
  appname = app->GetName();
  host = app->GetHost();
  printf("%s connected from %s\n", appname,  host);
}

void AggieCapTest :: OnApplicationDisconnected (IvyApplication *app)
{
  const char *appname;
  const char *host;
  appname = app->GetName ();
  host = app->GetHost();
  printf("%s disconnected from %s\n", appname,  host);
}

void AggieCapTest::OnApplicationCongestion(IvyApplication *app)
{
  std::cerr << "Ivy Congestion notififation\n";
}
void AggieCapTest::OnApplicationDecongestion(IvyApplication *app)
{
  std::cerr << "Ivy Decongestion notififation\n";
}
void AggieCapTest::OnApplicationFifoFull(IvyApplication *app)
{
  std::cerr << "Ivy FIFO Full  notififation : MESSAGE WILL BE LOST\n";
}


/**
 * This thread starts the Ivy Bus and the enters the IvyMainLoop
 */
void AggieCapTest::ivy_thread(AggieCapTest *test)
{
  test->Start();
}

/**
 *
    <message name="CAMERA_SNAPSHOT_DL" id="35" link="forwarded">
      <field name="ac_id" type="uint8"/>
      <field name="camera_id" type="uint16">Unique camera ID - consists of make,model and camera index</field>
      <field name="camera_state" type="uint8" values="UNKNOWN|OK|ERROR">State of the given camera</field>
      <field name="snapshot_image_number" type="uint16">Snapshot number in sequence</field>
      <field name="snapshot_valid" type="uint8" unit="bool">Flag checking whether the last snapshot was valid</field>
      <field name="lens_temp" type="float" unit="deg_celsius" format="%.2f">Lens temperature, NaN if not measured</field>
      <field name="array_temp" type="float" unit="deg_celsius" format="%.2f">Imager sensor temperature, NaN if not measured</field>
    </message>

    <message name="CAMERA_SNAPSHOT" id="128">
      <field name="camera_id" type="uint16">Unique camera ID - consists of make,model and camera index</field>
      <field name="camera_state" type="uint8" values="UNKNOWN|OK|ERROR">State of the given camera</field>
      <field name="snapshot_image_number" type="uint16">Snapshot number in sequence</field>
      <field name="snapshot_valid" type="uint8" unit="bool">Flag checking whether the last snapshot was valid</field>
      <field name="lens_temp" type="float" unit="deg_celsius" format="%.2f">Lens temperature, NaN if not measured</field>
      <field name="array_temp" type="float" unit="deg_celsius" format="%.2f">Imager sensor temperature, NaN if not measured</field>
    </message>
 */
void AggieCapTest::periodic_camera_snapshot(AggieCapTest *test)
{
  static uint camera_id = 12345;
  static uint camera_state = 0;
  static uint snapshot_image_number = 0;
  static uint snapshot_valid = 1;
  static float lens_temp = 30.1;
  static float array_temp = 33.3;

  while(true){
    if(test->debug_){
      test->bus->SendMsg("%s %s %u %u %u %u %f %f",
              test->name_.c_str(),
              test->camera_snapshot_.at(test->debug_).c_str(),
              camera_id, camera_state, snapshot_image_number, snapshot_valid, lens_temp, array_temp);
    }
    else {
      test->bus->SendMsg("%s %s %s %u %u %u %u %f %f",
              test->name_.c_str(),
              test->camera_snapshot_.at(test->debug_).c_str(),
              test->name_.c_str(), camera_id, camera_state, snapshot_image_number, snapshot_valid, lens_temp, array_temp);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));



    // increment variables
    camera_state++;
    camera_state = camera_state % 3;

    snapshot_image_number++;
  }
}


/**
 *
    <message name="CAMERA_PAYLOAD_DL" id="34" link="forwarded">
      <field name="ac_id" type="uint8"/>
      <field name="timestamp" type="float" unit="s">Payload computer timestamp</field>
      <field name="used_memory" type="uint8" unit="%">Percentage of used memory (RAM) of the payload computer rounded up to whole percent</field>
      <field name="used_disk" type="uint8" unit="%">Percentage of used disk of the payload computer rounded up to whole percent</field>
      <field name="door_status" type="uint8" values="UNKNOWN|CLOSE|OPEN">Payload door open/close</field>
      <field name="error_code" type="uint8" values="NONE|CAMERA_ERR|DOOR_ERR">Error codes of the payload</field>
    </message>

    <message name="CAMERA_PAYLOAD" id="111">
      <field name="timestamp" type="float" unit="s">Payload computer seconds since startup</field>
      <field name="used_memory" type="uint8" unit="%">Percentage of used memory (RAM) of the payload computer rounded up to whole percent</field>
      <field name="used_disk" type="uint8" unit="%">Percentage of used disk of the payload computer rounded up to whole percent</field>
      <field name="door_status" type="uint8" values="UNKNOWN|CLOSE|OPEN">Payload door open/close</field>
      <field name="error_code" type="uint8" values="NONE|CAMERA_ERR|DOOR_ERR">Error codes of the payload</field>
    </message>
 */
void AggieCapTest::periodic_camera_payload(AggieCapTest *test)
{
  static uint mem = 30;
  static uint disk = 60;
  static uint door = 1;
  static uint err = 0;

  while(true){
    if(test->debug_){
      test->bus->SendMsg("%s %s %f %u %u %u %u",
              test->name_.c_str(),
              test->camera_payload_.at(test->debug_).c_str(),
              test->sec_since_startup_, mem, disk, door, err);
    }
    else {
      test->bus->SendMsg("%s %s %s %f %u %u %u %u",
              test->name_.c_str(),
              test->camera_payload_.at(test->debug_).c_str(),
              test->name_.c_str(), test->sec_since_startup_, mem, disk, door, err);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // increment variables
    test->sec_since_startup_ += 2.0;
  }
}


/**
 *
    <message name="MOVE_WP" id="2" link="forwarded">
      <field name="wp_id" type="uint8"/>
      <field name="ac_id" type="uint8"/>
      <field name="lat" type="int32" unit="1e7deg" alt_unit="deg" alt_unit_coef="0.0000001"/>
      <field name="lon" type="int32" unit="1e7deg" alt_unit="deg" alt_unit_coef="0.0000001"/>
      <field name="alt" type="int32" unit="mm" alt_unit="m">Height above Mean Sea Level (geoid)</field>
    </message>
 */
void AggieCapTest::periodic_move_wp(AggieCapTest *test)
{
  static uint sender_id = 1;
  static uint ac_id = 1; // use AC_ID and include airframe.h
  static uint wp_id = 18; // normally we use WP_PAYLOAD here and include flight_plan.h
  static int lat = 418155620;
  static int lon = -1119824370;
  static int alt = 1350*1000; // 1350 m

  while(true){
    test->bus->SendMsg("%s MOVE_WP %u %u %d %d %d",
        test->name_.c_str(),wp_id, ac_id, lat, lon, alt);
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // increment variables
    alt = alt + (10*1000);
    lat = lat + 100;
  }
}


/**
 * Broadcast time information for time synchronization between components
    <message name="TIME" id="227">
      <field name="t" type="uint32">an integral value representing the number of seconds elapsed since 00:00 hours, Jan 1, 1970 UTC</field>
    </message>
 */
void AggieCapTest::periodic_send_time(AggieCapTest *test)
{
  time_t rawtime;

  while(true){
    // update time
    time (&rawtime);

    // broadcast
    test->bus->SendMsg("%s TIME %u", test->name_.c_str(),(uint32_t)rawtime);

    // sleep for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

}

void showhelpinfo(char *s) {
  cout<<"Usage:   "<<s<<" [-option] [argument]"<<endl;
  cout<<"option:  "<<"-h  show help information"<<endl;
  cout<<"         "<<"-b ivy bus (default is 127.255.255.255:2010)"<<endl;
  cout<<"         "<<"-d simulation mode on/off (default is false, use 'true' or '1')"<<endl;
  cout<<"         "<<"-n name (default is \"AggieCapTest\")"<<endl;
  cout<<"example: "<<s<<" -b 10.0.0.255:2010"<<endl;
}

int main(int argc, char** argv) {
  char tmp;
  char* ivy_bus = NULL;
  bool debug = false;
  char* name = NULL;

  while((tmp=getopt(argc,argv,"hb:d:n:"))!=-1)
  {
    switch(tmp)
    {
      /*option h show the help information*/
      case 'h':
        showhelpinfo(argv[0]);
        exit(1);
        break;
      case 'b':
        ivy_bus = optarg;
        cout << "-b " << ivy_bus << endl;
        break;
      case 'd':
        // check values of debug argument
        if (!strcmp("true",optarg)) {
          // equals true
          debug = true;
        }
        if (!strcmp("1",optarg)) {
          debug = true;
        }
        break;
      case 'n':
        name = optarg;
        break;
      /*do nothing on default*/
      default:
        break;
    }
  }

  AggieCapTest test(ivy_bus, debug, name);


  //Launch a thread
  std::thread t1(AggieCapTest::ivy_thread, &test);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::thread t2(AggieCapTest::periodic_camera_snapshot, &test);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::thread t3(AggieCapTest::periodic_camera_payload, &test);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::thread t4(AggieCapTest::periodic_move_wp, &test);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::thread t5(AggieCapTest::periodic_send_time, &test);
  //Wait for the ivy_thread to end
  t1.join();

  return 0;
}
