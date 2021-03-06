#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "MPC.h"
#include "json.hpp"
#include "tools.h"

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.rfind("}]");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

int main() {
  uWS::Hub h;

  // MPC is initialized here!
  MPC mpc;

  Tools tool;
//  tool.test();

  h.onMessage([&mpc, &tool](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    string sdata = string(data).substr(0, length);
//    cout << sdata << endl;
    if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
      string s = hasData(sdata);
      if (s != "") {
        auto j = json::parse(s);
        string event = j[0].get<string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          vector<double> next_x = j[1]["ptsx"];
          vector<double> next_y = j[1]["ptsy"];
          double px = j[1]["x"];
          double py = j[1]["y"];
          double psi = j[1]["psi"];
          double v = j[1]["speed"];
          std::cout << " x,"<< px <<" y,"<< py <<" psi,"<< psi <<" v,"<< v <<std::endl;

          //handle latency
          static double last_delta = 0;
          static double last_a = 0;
          double dt = 0.1;
          const double Lf = 2.67;

          double mileswmeters = (1609.0/3600);
          px = px + v*mileswmeters *cos(psi)*dt;
          py = py + v*mileswmeters *sin(psi)*dt;
          psi = psi + (v*mileswmeters)/Lf * last_delta * dt;
          v = v + (last_a * dt)/1609.0 ;

          std::cout << "After latency:  x,"<< px <<" y,"<< py <<" psi,"<< psi <<" v,"<< v <<std::endl;

		  //transform to vehicle coordinate system
		  tool.transform_map_coord(next_x, next_y, px, py, psi);

		  Eigen::VectorXd coeffs = tool.polyfit(next_x, next_y, 3);

		  for(int i =0; i<next_x.size();i++ ){
			  next_y[i] = mpc.polyeval(coeffs,next_x[i]);
		  }

          vector<double> mpc_x;
          vector<double> mpc_y;
          Eigen::VectorXd state(6);
          //after coordinate transformation, the vehicle's position (0,0)
          double cte = mpc.polyeval(coeffs, 0)-0;
          //the derivative is coeffs[1] + (2 * coeffs[2] * x) + (3 * coeffs[3]* (x*x)),
          //since x==0,it's equal to coeffs[1]
          double epsi = 0-atan(double(coeffs[1]));
          state << 0, 0, 0, v, cte,epsi;
          auto vars = mpc.Solve(state, coeffs,mpc_x,mpc_y);
          last_delta = vars[6];
          last_a = vars[7];

          vars[6] = -vars[6];
//          std::cout << std::endl << " x,"<< vars[0] <<" y,"<< vars[1] <<" psi,"<< vars[2] <<" v,"<< vars[3] <<" cte," \
//        		  << vars[4] <<" epsi,"<< vars[5] <<" steer,"<< vars[6] <<" a,"<< vars[7] << std::endl<< std::endl;


          double steer_value = 0;
          double throttle_value = 0;

          steer_value = vars[6];
          throttle_value = vars[7];

          json msgJson;
          msgJson["steering_angle"] = steer_value / deg2rad(25);
          msgJson["throttle"] = throttle_value;


          msgJson["next_x"] = next_x;
          msgJson["next_y"] = next_y;
          msgJson["mpc_x"] = mpc_x;
          msgJson["mpc_y"] = mpc_y;

          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
//          std::cout << msg << std::endl;

          // Latency
          // The purpose is to mimic real driving conditions where
          // the car does actuate the commands instantly.
          //
          // Feel free to play around with this value but should be to drive
          // around the track with 100ms latency.
          //
          // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE
          // SUBMITTING.
          this_thread::sleep_for(chrono::milliseconds(100));
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
