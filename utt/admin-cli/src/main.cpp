// UTT Client API
//
// Copyright (c) 2020-2022 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the sub-component's license, as noted in the LICENSE
// file.

#include <iostream>
#include <string>
#include <sstream>

#include "admin.hpp"

void printHelp() {
  std::cout << "\nCommands:\n";
  std::cout << "deploy -- generates a privacy config and deploys the privacy and token contracts.\n";
  std::cout << "create-budget <user-id> <amount> -- requests creation of a privacy budget for a user.\n";
  std::cout << '\n';
}

struct CLIApp {
  grpc::ClientContext ctx;
  Admin::Connection conn;
  Admin::Channel chan;
  utt::Configuration config;
  utt::client::TestUserPKInfrastructure pki;
  bool deployed = false;

  CLIApp() {
    conn = Admin::newConnection();
    if (!conn) throw std::runtime_error("Failed to create admin connection!");

    chan = conn->adminChannel(&ctx);
    if (!chan) throw std::runtime_error("Failed to create admin streaming channel!");
  }

  ~CLIApp() {
    std::cout << "Closing admin streaming channel... ";
    chan->WritesDone();
    auto status = chan->Finish();
    std::cout << " Done.\n";
    // std::cout << "gRPC error code: " << status.error_code() << '\n';
    // std::cout << "gRPC error msg: " << status.error_message() << '\n';
    // std::cout << "gRPC error details: " << status.error_details() << '\n';
  }

  void deploy() {
    if (deployed) {
      std::cout << "The privacy app is already deployed.\n";
      return;
    }

    deployed = Admin::deployApp(chan);
  }

  void createBudgetCmd(const std::vector<std::string>& cmdTokens) {
    if (cmdTokens.size() != 3) {
      std::cout << "Usage: create-budget <user-id> <amount>\n";
      return;
    }
    const auto& user = cmdTokens[1];
    int amount = std::atoi(cmdTokens[2].c_str());
    if (amount <= 0) {
      std::cout << "Expected a positive mint amount!\n";
      return;
    }
    Admin::createPrivacyBudget(chan, user, (uint64_t)amount);
  }
};

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  try {
    utt::client::Initialize();

    CLIApp app;

    std::cout << "\nSample Privacy Admin CLI Application.\n";

    while (true) {
      std::cout << "\nEnter command (type 'h' for commands 'Ctr-D' to quit):\n > ";
      std::string cmd;
      std::getline(std::cin, cmd);

      if (std::cin.eof()) {
        std::cout << "Quitting...\n";
        break;
      }

      if (cmd == "h") {
        printHelp();
      } else if (cmd == "deploy") {
        app.deploy();
      } else if (!app.deployed) {
        std::cout << "You must first deploy the privacy application. Use the 'deploy' command.\n";
      } else {
        // Tokenize params
        std::vector<std::string> cmdTokens;
        std::string token;
        std::stringstream ss(cmd);
        while (std::getline(ss, token, ' ')) cmdTokens.emplace_back(token);
        if (cmdTokens.empty()) continue;

        if (cmdTokens[0] == "create-budget") {
          app.createBudgetCmd(cmdTokens);
        } else {
          std::cout << "Unknown command '" << cmd << "'\n";
        }
      }
    }
  } catch (const std::runtime_error& e) {
    std::cout << "Error (exception): " << e.what() << '\n';
    return 1;
  }

  return 0;
}