#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <json.hpp>
#include <queue>

#include <regex>
#include <signal.h>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

#include "App.h"
#include "Consts.h"
#include "Game.h"
#include "Player.h"
#include "StringUtils.h"

#define JWT_DISABLE_PICOJSON true
#include "defaults.h"
// for convenience
using json = nlohmann::json;

using time_point = std::chrono::system_clock::time_point;
std::string serializeTimePoint(const time_point &time,
                               const std::string &format) {
    std::time_t tt = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::gmtime(&tt); // GMT (UTC)
    std::stringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str();
}

std::unordered_map<std::string, Game *> games;

std::unordered_set<std::string> eviction_set;
std::queue<std::pair<std::chrono::system_clock::time_point, std::string>>
    eviction_queue;

typedef uWS::HttpResponse<false> HttpResponse;
typedef uWS::HttpRequest HttpRequest;

void load_persistence() {
    std::ifstream state_file("server_state.json");
    if (!state_file.is_open()) return;
    json state;
    state_file >> state;
    state_file.close();
    std::cout << "Successfully parsed state file! Rehydrating..." << std::endl;
    for (auto &room : state.items()) {
        std::cout << "Restoring room '" << room.key() << "'" << std::endl;
        Game *g = new Game(room.value());
        games.insert({room.key(), g});

        // All rehydrated games start with 0 players, so we can schedule an eviction.
        eviction_queue.push({std::chrono::system_clock::now(), room.key()});
        eviction_set.insert(room.key());
    }
}

void runEviction(bool limited = true) {
    bool popSet = true;
    uint kills = 0;
    while (!eviction_queue.empty()) {
        if (kills >= EVICTION_LIMIT && limited)
            break;
        auto i = eviction_queue.front();
        if (i.first < std::chrono::system_clock::now() - EVICT_AFTER) {
            auto it = games.find(i.second);
            if (it != games.end()) {
                Game *g = it->second;
                if (!g->connectedPlayerCount()) {
                    if (g->getUpdated() <= i.first) {
                        games.erase(it);
                        delete g;
                        ++kills;
                    } else {
                        eviction_queue.push(
                            {std::chrono::system_clock::now(), i.second});
                        popSet = false;
                    }
                }
            }
            if (popSet) {
                eviction_set.erase(i.second);
            }
            eviction_queue.pop();
        } else {
            break;
        }
    }
    std::cout << "Evicted " << kills << " games" << std::endl;
}

void signal_callback_handler(int signum) {
    std::cout << "Caught signal, attempting graceful shutdown..." << std::endl;
    runEviction(false);
    json state;
    for (const auto &g : games) {
        state[g.first] = g.second->toJson(true);
    }
    remove("server_state.json");
    std::ofstream state_file("server_state.json");
    if (!state.is_null()) {
        state_file << state;
        state_file.close();
        std::cout << "Successfully persisted state to disk." << std::endl;
    }
    exit(0);
}

std::string getSession(HttpRequest *req) {
    const std::regex sessionRegex("_session=([^;]*)");
    std::smatch cookies;
    std::string s(req->getHeader("cookie"));
    std::regex_search(s, cookies, sessionRegex);
    return "guest:" + std::string(cookies[1].str());
}

std::string createRoom(bool isPrivate, std::string seed = "") {
    runEviction();
    std::string id;
    do {
        id = generateCode(ROOM_LEN, seed);
        if (games.find(id) != games.end() && seed != "") {
            // Short circuit to prevent infinite loop
            // in the case of a seeded redirect already
            // existing
            return id;
        }
    } while (games.find(id) != games.end());
    Game *g = new Game(isPrivate);
    games.insert({id, g});
    if (!eviction_set.count(id)) {
        eviction_queue.push({std::chrono::system_clock::now(), id});
        eviction_set.insert(id);
    }
    return id;
}

void connectNewPlayer(uWS::App *app, uWS::WebSocket<false, true, PerSocketData> *ws, PerSocketData *userData) {
    auto it = games.find(userData->room);
    if (it != games.end()) {
        // Connecting to a valid game
        Game *g = it->second;
        if (!userData->spectator) {
            if (!g->hasPlayer(userData->session)) {
                json resp = g->addPlayer(*userData);
                if (resp.is_null()) {
                    // room is full
                    ws->close();
                    return;
                }
                app->publish(userData->room, resp.dump(), uWS::OpCode::TEXT);
            } else {
                json resp = g->reconnectPlayer(userData->session);
                app->publish(userData->room, resp.dump(), uWS::OpCode::TEXT);
            }
        }
        json welcome;
        welcome["type"] = "welcome";
        welcome["game"] = g->toJson();
        if (!userData->spectator) welcome["id"] = g->getPlayerId(userData->session);
        ws->send(welcome.dump(), uWS::OpCode::TEXT);
        ws->subscribe(userData->room);
    } else if (userData->spectator) {
        ws->close();
        return;
    } else {
        // Connecting to a non-existant room
        // let's migrate them to a new room
        userData->room = createRoom(true, userData->room);
        json route;
        route["type"] = "redirect";
        route["room"] = userData->room;
        ws->send(route.dump(), uWS::OpCode::TEXT);
    }
}

auto setup_token_validator() {
    // TODO: fetch this from http://localhost:3031/public_key
    std::string public_key = R"(-----BEGIN PUBLIC KEY-----
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAxAs+hVnJOO08u2yB2d1/
z8kooCSeweTc+Z1yAQvoIYIT7F67OXNiuS5Fhq5Q5hiQY71sh+rGKK72GyjQ1OM7
eEnGpuZyMEhrnCPEvBnoSJett0gxLqiqFytUK96k8x/b9okcNs2B21T0zLRGGOoZ
X1Anq4LOQDJ8ZHKel3+nLsmo9ertEs6CfMJfewgoRXhJSWxONSv1wSHIMPNyo1VK
UG2/yVN9n/F1aRn+DBvG7OpnYS1RIlGwSz6ab1DBbwrznxPl8l/+u43xbL4n5PLe
xwP5I4324dyO4teV5TiX0C2/OSv2D2aFIKDpTkcm6+0KnQIuIbNvzY1RaoDB1Oy1
FkGm+FHGn/PtasqU7GXK3rDry75EDcWOnkVn41/AgBEzTp7oRRNAtIGAmAhfLTHP
KSjmTC/blI+eosSuULQ58tEdZSauZXka5l43ikQMWERtMd8nIrLZaLFG/WCDypqm
gics4A0uoucodwHqkd/wdOn9nbNCGj8ruhJXJFi++dejEZRUybMhAOLf6oAMS/6k
wD5UbFt1UoZuMHSzEUzcKaLpoCCjNg6Jxc5VTBHuTuzNiHJ8AULw6CfrUjO3yJ9I
/sDRNfe0wnJbvGLYNzeXi5AxBacfqkiUD4cAZExkPKi2gGyNzyH7Jem+Yd/a7pAQ
p0UHBGyVNjRYF86YetoiFisCAwEAAQ==
-----END PUBLIC KEY-----)";

    auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::rs256(public_key, "", "", ""));

    return verifier;
}

typedef jwt::verifier<jwt::default_clock, jwt::traits::nlohmann_json> JWTVerifier;

uWS::App::WebSocketBehavior<PerSocketData> makeWebsocketBehavior(uWS::App *app, const JWTVerifier &jwt_verifier) {
    return {/* Settings */
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            /* Handlers */
            .upgrade =
                [](auto *res, auto *req, auto *context) {
                    std::string session = getSession(req);
                    bool is_verified = true;
                    std::string mode = std::string(req->getParameter(0));
                    std::string room = std::string(req->getParameter(1));
                    std::string user_id = std::string(req->getQuery("userId"));
                    if (user_id.length()) {
                        session = user_id;
                        is_verified = false;
                    }
                    bool dedupe_conns = false;
                    auto it = games.find(room);
                    if (it != games.end()) {
                        // Connecting to a valid game
                        Game *g = it->second;
                        if (g->isPlayerConnected(session)) {
                            dedupe_conns = true;
                        }
                    }
                    res->template upgrade<PerSocketData>(
                        {.session = session,
                         .room = room,
                         .is_verified = is_verified,
                         .spectator = (mode == "spectate"),
                         .dedupe_conns = dedupe_conns},
                        req->getHeader("sec-websocket-key"),
                        req->getHeader("sec-websocket-protocol"),
                        req->getHeader("sec-websocket-extensions"),
                        context);
                },
            .open =
                [app](auto *ws) {
                    PerSocketData *userData =
                        (PerSocketData *)ws->getUserData();

                    if (userData->session == "") {
                        ws->close();
                        return;
                    }
                    if (userData->dedupe_conns) {
                        json resp;
                        resp["error"] = "already connected";
                        ws->send(resp.dump(), uWS::OpCode::TEXT);
                        return;
                    }
                    if (!userData->is_verified) {
                        // We shouldn't mutate the game state until we're sure this user is
                        // real.
                        return;
                    }
                    connectNewPlayer(app, ws, userData);
                },
            .message =
                [app, jwt_verifier](auto *ws, std::string_view message, uWS::OpCode opCode) {
                    PerSocketData *userData =
                        (PerSocketData *)ws->getUserData();

                    if (userData->dedupe_conns) return;
                    if (userData->spectator) return;

                    std::string room = userData->room;
                    std::string session = userData->session;
                    json response;
                    try {
                        auto data = json::parse(message);
                        if (!data["type"].is_string())
                            throw GameError("Type is not specified correctly");
                        auto action_type = data["type"].get<std::string>();
                        if (action_type == "authenticate") {
                            auto token = data["access_token"].get<std::string>();
                            std::cout << "RECEIVED TOKEN" << std::endl;
                            try {
                                auto decoded = jwt::decode(token);
                                jwt_verifier.verify(decoded);
                                userData->is_verified = true;
                                std::cout << "valid token!" << std::endl;
                                auto claims = decoded.get_payload_claims();
                                userData->user_id = claims["user_id"].to_json().get<std::string>();
                                userData->display_name = claims["display_name"].to_json().get<std::string>();
                                connectNewPlayer(app, ws, userData);
                            } catch (const jwt::error::signature_verification_exception &e) {
                                std::cout << "signature verification exception" << std::endl;
                                ws->close();
                            } catch (const jwt::error::token_verification_exception &e) {
                                std::cout << "token verification exception" << std::endl;
                                ws->close();
                            } catch (const std::runtime_error &e) {
                                // Probably a parsing error
                                std::cout << "token parsing error" << std::endl;
                                ws->close();
                            }
                        } else {
                            // Toss out any messages from unverified connection
                            if (!userData->is_verified) return;
                            auto it = games.find(room);
                            if (it != games.end()) {
                                Game *g = it->second;
                                try {
                                    g->handleMessage(
                                        [&ws](auto s) { ws->send(s, uWS::OpCode::TEXT); },
                                        [&ws, &room, app](auto s) {
                                            // ws->send(s, uWS::OpCode::TEXT);
                                            app->publish(room, s, uWS::OpCode::TEXT);
                                        },
                                        data, session);
                                    /*if (data["type"].is_string()) {
                                  if (data["type"] == "leave") {
                                  ws->close();
                                  }
                                  }*/
                                } catch (GameError &e) {
                                    response["error"] = e.what();
                                }
                            } else {
                                response["error"] = "Room not found: " + room;
                            }
                        }
                    } catch (nlohmann::detail::parse_error &e) {
                        std::cout << "RECEIVED BAD JSON (parse_error): " << message
                                  << std::endl
                                  << e.what() << std::endl;
                        response["error"] = e.what();
                    } catch (nlohmann::detail::type_error &e) {
                        std::cout << "HANDLED BAD JSON (type_error): " << message
                                  << std::endl
                                  << e.what() << std::endl;
                        response["error"] = e.what();
                    }
                    if (!response.is_null())
                        ws->send(response.dump(), uWS::OpCode::TEXT);
                },
            .close =
                [app](auto *ws, int code, std::string_view message) {
                    PerSocketData *userData =
                        (PerSocketData *)ws->getUserData();

                    if (userData->spectator) return;
                    if (userData->dedupe_conns) return;

                    std::string room = userData->room;
                    std::string session = userData->session;
                    auto it = games.find(room);
                    if (it != games.end()) {
                        Game *g = it->second;
                        json resp = g->disconnectPlayer(session);
                        if (!resp.is_null()) {
                            app->publish(room, resp.dump(), uWS::OpCode::TEXT);
                        }
                        if (!g->connectedPlayerCount()) {
                            if (!eviction_set.count(room)) {
                                eviction_queue.push(
                                    {std::chrono::system_clock::now(), room});
                                eviction_set.insert(room);
                            }
                        }
                    }
                }};
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);
    signal(SIGTERM, signal_callback_handler);

    int port = DEFAULT_PORT;
    if (argc == 2) {
        // try to read a port as arg 2
        port = atoi(argv[1]);
    }
    try {
        load_persistence();
    } catch (const std::exception &e) {
        std::cout << "WARNING: Failed to load server state from persistence!" << std::endl;
        std::cout << e.what() << std::endl;
    }

    auto jwt_verifier = setup_token_validator();

    uWS::App app;
    app.get("/list",
            [](auto *res, auto *req) {
                res->writeHeader("Content-Type", "application/json");
                json respList = json::array();
                for (auto const &[code, game] : games) {
                    if (game->isPrivate()) {
                        continue;
                    }
                    std::string updated = serializeTimePoint(
                        game->getUpdated(), "UTC: %Y-%m-%d %H:%M:%S");
                    respList.push_back(
                        {{"code", code},
                         {"playerCount", game->connectedPlayerCount()},
                         {"lastUpdated", updated},
                         {"hostName", game->hostName()}});
                }
                res->write(respList.dump());
                res->end();
            })
        .get("/create",
             [](auto *res, auto *req) {
                 std::string session = getSession(req);
                 bool isPrivate = true;
                 if (req->getQuery().find("public") != std::string::npos) {
                     isPrivate = false;
                 }
                 if (session == "") {
                     res->end();
                 } else {
                     res->end(createRoom(isPrivate));
                 }
             })
        .ws<PerSocketData>(
            "/ws/:mode/:room",
            makeWebsocketBehavior(&app, jwt_verifier))
        .listen(port,
                [port](auto *token) {
                    if (token) {
                        std::cout << "Listening on port " << port << std::endl;
                    }
                })
        .run();

    std::cout << "Failed to listen on port " << port << std::endl;
}
