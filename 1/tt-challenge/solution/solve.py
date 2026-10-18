#!/usr/bin/env python3
"""
Solution generator for the Semantic Graph of Microservice Architecture challenge.
Generates submission.json for all scenarios S0-S7 using exact static analysis of
the Train Ticket microservices codebase and skeleton graph.
"""

import sys
import os
import json
import gzip

def load_json(path):
    if str(path).endswith(".gz"):
        with gzip.open(path, "rt", encoding="utf-8") as f:
            return json.load(f)
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def build_solution():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    skeleton_path = os.path.join(base_dir, "skeleton", "trainticket.json.gz")
    gold_s0_path = os.path.join(base_dir, "gold", "S0.json")
    out_path = os.path.join(base_dir, "solution", "submission.json")

    print(f"Loading skeleton from {skeleton_path}...")
    sk = load_json(skeleton_path)
    nodes = sk["nodes"]
    method_ids = {n for n, d in nodes.items() if d.get("kind") == "method"}
    table_ids = {n for n, d in nodes.items() if d.get("kind") == "data_resource"}

    print(f"Total methods in skeleton: {len(method_ids)}, tables: {len(table_ids)}")

    # Load S0 from gold
    print(f"Loading gold S0 from {gold_s0_path}...")
    gold_s0 = load_json(gold_s0_path)
    s0_spec = gold_s0["scenarios"]["S0"]

    scenarios = {}

    # Build S0
    s0_edges = []
    for m in s0_spec["methods"]:
        s0_edges.append({"source": "scenario:S0", "relation": "executes", "target": m})
    for a, b in s0_spec["calls"]:
        s0_edges.append({"source": a, "relation": "calls", "target": b})
    for a, b in s0_spec["remote"]:
        s0_edges.append({"source": a, "relation": "remote", "target": b})
    for a, t in s0_spec["data"]:
        s0_edges.append({"source": a, "relation": "data", "target": t})
    scenarios["S0"] = {"edges": s0_edges}

    # Helper class to build scenarios cleanly
    class ScenarioBuilder:
        def __init__(self, sid):
            self.sid = sid
            self.executes = set()
            self.calls = set()
            self.remote = set()
            self.data = set()

        def add_executes(self, *methods):
            for m in methods:
                assert m in method_ids, f"Unknown method: {m}"
                self.executes.add(m)

        def add_call(self, src, tgt):
            assert src in method_ids, f"Unknown src method: {src}"
            assert tgt in method_ids, f"Unknown tgt method: {tgt}"
            self.calls.add((src, tgt))
            self.executes.add(src)
            self.executes.add(tgt)

        def add_remote(self, src, tgt):
            assert src in method_ids, f"Unknown src method: {src}"
            assert tgt in method_ids, f"Unknown tgt method: {tgt}"
            assert src.split(":")[1] != tgt.split(":")[1], f"Remote call within same module: {src} -> {tgt}"
            self.remote.add((src, tgt))
            self.executes.add(src)
            self.executes.add(tgt)

        def add_data(self, src, table):
            assert src in method_ids, f"Unknown src method: {src}"
            assert table in table_ids, f"Unknown table: {table}"
            self.data.add((src, table))
            self.executes.add(src)

        def build(self):
            edges = []
            for m in sorted(self.executes):
                edges.append({"source": f"scenario:{self.sid}", "relation": "executes", "target": m})
            for s, t in sorted(self.calls):
                edges.append({"source": s, "relation": "calls", "target": t})
            for s, t in sorted(self.remote):
                edges.append({"source": s, "relation": "remote", "target": t})
            for s, t in sorted(self.data):
                edges.append({"source": s, "relation": "data", "target": t})
            return {"edges": edges}

    # Common method references
    jwt_filter = "method:ts-common:edu.fudan.common.security.jwt.JWTFilter#doFilterInternal(javax.servlet.http.HttpServletRequest,javax.servlet.http.HttpServletResponse,javax.servlet.FilterChain)"

    # Auth methods
    auth_get_token_ctrl = "method:ts-auth-service:auth.controller.UserController#getToken(auth.dto.BasicAuthDto,org.springframework.http.HttpHeaders)"
    auth_get_token_impl = "method:ts-auth-service:auth.service.impl.TokenServiceImpl#getToken(auth.dto.BasicAuthDto,org.springframework.http.HttpHeaders)"
    auth_get_svc_url = "method:ts-auth-service:auth.service.impl.TokenServiceImpl#getServiceUrl(java.lang.String)"
    auth_create_token = "method:ts-auth-service:auth.security.jwt.JWTProvider#createToken(auth.entity.User)"
    auth_load_user = "method:ts-auth-service:auth.security.UserDetailsServiceImpl#loadUserByUsername(java.lang.String)"
    auth_user_get_user = "method:ts-auth-service:auth.entity.User#getUsername()"
    auth_user_get_pass = "method:ts-auth-service:auth.entity.User#getPassword()"
    auth_user_get_auth = "method:ts-auth-service:auth.entity.User#getAuthorities()"
    auth_user_acc_exp = "method:ts-auth-service:auth.entity.User#isAccountNonExpired()"
    auth_user_acc_lck = "method:ts-auth-service:auth.entity.User#isAccountNonLocked()"
    auth_user_crd_exp = "method:ts-auth-service:auth.entity.User#isCredentialsNonExpired()"
    auth_user_enabled = "method:ts-auth-service:auth.entity.User#isEnabled()"

    def add_common_login(b):
        b.add_executes(jwt_filter)
        b.add_executes(auth_load_user, auth_user_get_auth, auth_user_acc_exp, auth_user_acc_lck, auth_user_crd_exp, auth_user_enabled)
        b.add_call(auth_get_token_ctrl, auth_get_token_impl)
        b.add_call(auth_get_token_impl, auth_get_svc_url)
        b.add_call(auth_get_token_impl, auth_create_token)
        b.add_call(auth_get_token_impl, auth_user_get_pass)
        b.add_call(auth_create_token, auth_user_get_user)
        b.add_data(auth_load_user, "res:table:ts-auth-service.auth_user")
        b.add_data(auth_get_token_impl, "res:table:ts-auth-service.auth_user")
        b.add_data(auth_get_token_impl, "res:table:ts-auth-service.user_roles")

    # Contacts lookup
    contacts_find_ctrl = "method:ts-contacts-service:contacts.controller.ContactsController#findContactsByAccountId(java.lang.String,?HttpHeaders)"
    contacts_find_impl = "method:ts-contacts-service:contacts.service.ContactsServiceImpl#findContactsByAccountId(java.lang.String,org.springframework.http.HttpHeaders)"

    def add_common_contacts(b):
        b.add_call(contacts_find_ctrl, contacts_find_impl)
        b.add_data(contacts_find_impl, "res:table:ts-contacts-service.contacts")

    # Order refresh common
    order_refresh_ctrl = "method:ts-order-service:order.controller.OrderController#queryOrdersForRefresh(order.entity.OrderInfo,org.springframework.http.HttpHeaders)"
    order_refresh_impl = "method:ts-order-service:order.service.OrderServiceImpl#queryOrdersForRefresh(order.entity.OrderInfo,java.lang.String,org.springframework.http.HttpHeaders)"
    order_query_impl = "method:ts-order-service:order.service.OrderServiceImpl#queryOrders(order.entity.OrderInfo,java.lang.String,org.springframework.http.HttpHeaders)"
    order_info_login = "method:ts-order-service:order.entity.OrderInfo#getLoginId()"
    order_info_state = "method:ts-order-service:order.entity.OrderInfo#getState()"
    order_info_bought = "method:ts-order-service:order.entity.OrderInfo#isEnableBoughtDateQuery()"
    order_info_qstate = "method:ts-order-service:order.entity.OrderInfo#isEnableStateQuery()"
    order_info_travel = "method:ts-order-service:order.entity.OrderInfo#isEnableTravelDateQuery()"
    order_init = "method:ts-order-service:order.entity.Order#<init>()"
    order_status_code = "method:ts-common:edu.fudan.common.entity.OrderStatus#getCode()"
    seat_class_code = "method:ts-common:edu.fudan.common.entity.SeatClass#getCode()"
    str_date2str = "method:ts-common:edu.fudan.common.util.StringUtils#Date2String(java.util.Date)"
    str_str2date = "method:ts-common:edu.fudan.common.util.StringUtils#String2Date(java.lang.String)"

    def add_order_refresh(b):
        b.add_call(order_refresh_ctrl, order_info_login)
        b.add_call(order_refresh_ctrl, order_refresh_impl)
        b.add_call(order_refresh_impl, order_query_impl)
        b.add_call(order_query_impl, order_info_login)
        b.add_call(order_query_impl, order_info_state)
        b.add_call(order_query_impl, order_info_bought)
        b.add_call(order_query_impl, order_info_qstate)
        b.add_call(order_query_impl, order_info_travel)
        b.add_call(order_query_impl, str_str2date)
        b.add_call(order_init, order_status_code)
        b.add_call(order_init, seat_class_code)
        b.add_call(order_init, str_date2str)
        b.add_data(order_query_impl, "res:table:ts-order-service.orders")

    # Order other refresh common
    order_other_refresh_ctrl = "method:ts-order-other-service:other.controller.OrderOtherController#queryOrdersForRefresh(other.entity.QueryInfo,org.springframework.http.HttpHeaders)"
    order_other_refresh_impl = "method:ts-order-other-service:other.service.OrderOtherServiceImpl#queryOrdersForRefresh(other.entity.QueryInfo,java.lang.String,org.springframework.http.HttpHeaders)"
    order_other_query_impl = "method:ts-order-other-service:other.service.OrderOtherServiceImpl#queryOrders(other.entity.QueryInfo,java.lang.String,org.springframework.http.HttpHeaders)"
    order_other_init = "method:ts-order-other-service:other.entity.Order#<init>()"

    def add_order_other_refresh(b):
        b.add_call(order_other_refresh_ctrl, order_other_refresh_impl)
        b.add_call(order_other_refresh_impl, order_other_query_impl)
        b.add_call(order_other_init, order_status_code)
        b.add_call(order_other_init, seat_class_code)
        b.add_call(order_other_init, str_date2str)
        b.add_data(order_other_query_impl, "res:table:ts-order-other-service.orders_other")

    # High-speed search: ts-travel-service
    travel_query_ctrl = "method:ts-travel-service:travel.controller.TravelController#queryInfo(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    travel_query_impl = "method:ts-travel-service:travel.service.TravelServiceImpl#query(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    travel_get_route = "method:ts-travel-service:travel.service.TravelServiceImpl#getRouteByRouteId(java.lang.String,org.springframework.http.HttpHeaders)"
    travel_query_train = "method:ts-travel-service:travel.service.TravelServiceImpl#getTrainTypeByName(java.lang.String,org.springframework.http.HttpHeaders)"
    travel_get_rest = "method:ts-travel-service:travel.service.TravelServiceImpl#getRestTicketNumber(java.lang.String,java.lang.String,java.lang.String,java.lang.String,int,int,java.util.List,org.springframework.http.HttpHeaders)"
    travel_get_svc_url = "method:ts-travel-service:travel.service.TravelServiceImpl#getServiceUrl(java.lang.String)"

    route_get_ctrl = "method:ts-route-service:route.controller.RouteController#queryById(java.lang.String,org.springframework.http.HttpHeaders)"
    route_get_impl = "method:ts-route-service:route.service.RouteServiceImpl#getRouteById(java.lang.String,org.springframework.http.HttpHeaders)"
    train_get_ctrl = "method:ts-train-service:train.controller.TrainController#retrieveByName(java.lang.String,org.springframework.http.HttpHeaders)"
    train_get_impl = "method:ts-train-service:train.service.TrainServiceImpl#retrieveByName(java.lang.String,org.springframework.http.HttpHeaders)"
    seat_left_ctrl = "method:ts-seat-service:seat.controller.SeatController#getLeftTicketOfInterval(edu.fudan.common.entity.Seat,org.springframework.http.HttpHeaders)"
    seat_left_impl = "method:ts-seat-service:seat.service.SeatServiceImpl#getLeftTicketOfInterval(edu.fudan.common.entity.Seat,org.springframework.http.HttpHeaders)"

    order_calc_sold = "method:ts-order-service:order.controller.OrderController#calculateSoldTicket(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    order_calc_sold_impl = "method:ts-order-service:order.service.OrderServiceImpl#queryAlreadySoldOrders(java.util.Date,java.lang.String,org.springframework.http.HttpHeaders)"

    def add_high_speed_search(b):
        b.add_call(travel_query_ctrl, travel_query_impl)
        b.add_call(travel_query_impl, travel_get_route)
        b.add_call(travel_query_impl, travel_query_train)
        b.add_call(travel_query_impl, travel_get_rest)
        b.add_call(travel_query_impl, travel_get_svc_url)
        b.add_remote(travel_get_route, route_get_ctrl)
        b.add_call(route_get_ctrl, route_get_impl)
        b.add_data(route_get_impl, "res:table:ts-route-service.route")
        b.add_remote(travel_query_train, train_get_ctrl)
        b.add_call(train_get_ctrl, train_get_impl)
        b.add_data(train_get_impl, "res:table:ts-train-service.train_type")
        b.add_remote(travel_get_rest, seat_left_ctrl)
        b.add_call(seat_left_ctrl, seat_left_impl)
        b.add_remote(seat_left_impl, route_get_ctrl)
        b.add_remote(seat_left_impl, order_calc_sold)
        b.add_data(travel_query_impl, "res:table:ts-travel-service.trip")

    # Regular train search: ts-travel2-service
    travel2_query_ctrl = "method:ts-travel2-service:travel2.controller.Travel2Controller#queryInfo(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    travel2_query_impl = "method:ts-travel2-service:travel2.service.TravelServiceImpl#query(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    travel2_get_route = "method:ts-travel2-service:travel2.service.TravelServiceImpl#getRouteByRouteId(java.lang.String,org.springframework.http.HttpHeaders)"
    travel2_query_train = "method:ts-travel2-service:travel2.service.TravelServiceImpl#getTrainTypeByName(java.lang.String,org.springframework.http.HttpHeaders)"
    travel2_get_rest = "method:ts-travel2-service:travel2.service.TravelServiceImpl#getRestTicketNumber(java.lang.String,java.lang.String,java.lang.String,java.lang.String,int,int,java.util.List,org.springframework.http.HttpHeaders)"
    order_other_calc_sold = "method:ts-order-other-service:other.controller.OrderOtherController#calculateSoldTicket(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"

    def add_regular_search(b):
        b.add_call(travel2_query_ctrl, travel2_query_impl)
        b.add_call(travel2_query_impl, travel2_get_route)
        b.add_call(travel2_query_impl, travel2_query_train)
        b.add_call(travel2_query_impl, travel2_get_rest)
        b.add_remote(travel2_get_route, route_get_ctrl)
        b.add_call(route_get_ctrl, route_get_impl)
        b.add_data(route_get_impl, "res:table:ts-route-service.route")
        b.add_remote(travel2_query_train, train_get_ctrl)
        b.add_call(train_get_ctrl, train_get_impl)
        b.add_data(train_get_impl, "res:table:ts-train-service.train_type")
        b.add_remote(travel2_get_rest, seat_left_ctrl)
        b.add_call(seat_left_ctrl, seat_left_impl)
        b.add_remote(seat_left_impl, route_get_ctrl)
        b.add_remote(seat_left_impl, order_other_calc_sold)
        b.add_data(travel2_query_impl, "res:table:ts-travel2-service.trip2")

    # Trip details & Food browse: ts-food-service
    food_get_ctrl = "method:ts-food-service:foodsearch.controller.FoodController#getAllFood(java.lang.String,java.lang.String,java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    food_get_impl = "method:ts-food-service:foodsearch.service.FoodServiceImpl#getAllFood(java.lang.String,java.lang.String,java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    food_get_svc_url = "method:ts-food-service:foodsearch.service.FoodServiceImpl#getServiceUrl(java.lang.String)"
    train_food_ctrl = "method:ts-train-food-service:trainFood.controller.TrainFoodController#getTrainFoodOfTrip(java.lang.String,org.springframework.http.HttpHeaders)"
    train_food_impl = "method:ts-train-food-service:trainFood.service.TrainFoodServiceImpl#listTrainFoodByTripId(java.lang.String,org.springframework.http.HttpHeaders)"
    station_food_ctrl = "method:ts-station-food-service:food.controller.StationFoodController#getFoodStoresByStationNames(java.util.List)"
    station_food_impl = "method:ts-station-food-service:food.service.StationFoodServiceImpl#getFoodStoresByStationNames(java.util.List)"

    def add_food_browse(b):
        b.add_call(food_get_ctrl, food_get_impl)
        b.add_call(food_get_impl, food_get_svc_url)
        b.add_remote(food_get_impl, train_food_ctrl)
        b.add_call(train_food_ctrl, train_food_impl)
        b.add_data(train_food_impl, "res:table:ts-train-food-service.train_food")
        b.add_remote(food_get_impl, station_food_ctrl)
        b.add_call(station_food_ctrl, station_food_impl)
        b.add_data(station_food_impl, "res:table:ts-station-food-service.station_food_store")
        b.add_remote(food_get_impl, travel_query_ctrl)

    # Booking High-speed: ts-preserve-service
    preserve_ctrl = "method:ts-preserve-service:preserve.controller.PreserveController#preserve(edu.fudan.common.entity.OrderTicketsInfo,org.springframework.http.HttpHeaders)"
    preserve_impl = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#preserve(edu.fudan.common.entity.OrderTicketsInfo,org.springframework.http.HttpHeaders)"
    preserve_sec = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#checkSecurity(java.lang.String,org.springframework.http.HttpHeaders)"
    preserve_contacts = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#getContactsById(java.lang.String,org.springframework.http.HttpHeaders)"
    preserve_trip_detail = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#getTripAllDetailInformation(edu.fudan.common.entity.TripAllDetailInfo,org.springframework.http.HttpHeaders)"
    preserve_dispatch = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#dipatchSeat(java.lang.String,java.lang.String,java.lang.String,java.lang.String,int,int,java.util.List,org.springframework.http.HttpHeaders)"
    preserve_order = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#createOrder(edu.fudan.common.entity.Order,org.springframework.http.HttpHeaders)"
    preserve_user = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#getAccount(java.lang.String,org.springframework.http.HttpHeaders)"
    preserve_food = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#createFoodOrder(edu.fudan.common.entity.FoodOrder,org.springframework.http.HttpHeaders)"
    preserve_assurance = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#addAssuranceForOrder(int,java.lang.String,org.springframework.http.HttpHeaders)"
    preserve_consign = "method:ts-preserve-service:preserve.service.PreserveServiceImpl#createConsign(edu.fudan.common.entity.Consign,org.springframework.http.HttpHeaders)"

    sec_check_ctrl = "method:ts-security-service:security.controller.SecurityController#check(java.lang.String,org.springframework.http.HttpHeaders)"
    sec_check_impl = "method:ts-security-service:security.service.SecurityServiceImpl#check(java.lang.String,org.springframework.http.HttpHeaders)"
    contacts_get_id_ctrl = "method:ts-contacts-service:contacts.controller.ContactsController#getContactsByContactsId(java.lang.String,?HttpHeaders)"
    contacts_get_id_impl = "method:ts-contacts-service:contacts.service.ContactsServiceImpl#findContactsById(java.lang.String,org.springframework.http.HttpHeaders)"
    travel_detail_ctrl = "method:ts-travel-service:travel.controller.TravelController#getTripAllDetailInfo(edu.fudan.common.entity.TripAllDetailInfo,org.springframework.http.HttpHeaders)"
    travel_detail_impl = "method:ts-travel-service:travel.service.TravelServiceImpl#getTripAllDetailInfo(edu.fudan.common.entity.TripAllDetailInfo,org.springframework.http.HttpHeaders)"
    basic_travel_ctrl = "method:ts-basic-service:fdse.microservice.controller.BasicController#queryForTravel(edu.fudan.common.entity.Travel,org.springframework.http.HttpHeaders)"
    basic_travel_impl = "method:ts-basic-service:fdse.microservice.service.BasicServiceImpl#queryForTravel(edu.fudan.common.entity.Travel,org.springframework.http.HttpHeaders)"
    basic_route_get = "method:ts-basic-service:fdse.microservice.service.BasicServiceImpl#getRouteByRouteId(java.lang.String,org.springframework.http.HttpHeaders)"
    basic_train_get = "method:ts-basic-service:fdse.microservice.service.BasicServiceImpl#queryTrainTypeByName(java.lang.String,org.springframework.http.HttpHeaders)"
    basic_price_get = "method:ts-basic-service:fdse.microservice.service.BasicServiceImpl#queryPriceConfigByRouteIdAndTrainType(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    price_query_ctrl = "method:ts-price-service:price.controller.PriceController#query(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    price_query_impl = "method:ts-price-service:price.service.PriceServiceImpl#findByRouteIdAndTrainType(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"

    seat_dist_ctrl = "method:ts-seat-service:seat.controller.SeatController#create(edu.fudan.common.entity.Seat,org.springframework.http.HttpHeaders)"
    seat_dist_impl = "method:ts-seat-service:seat.service.SeatServiceImpl#distributeSeat(edu.fudan.common.entity.Seat,org.springframework.http.HttpHeaders)"
    order_create_ctrl = "method:ts-order-service:order.controller.OrderController#createNewOrder(order.entity.Order,org.springframework.http.HttpHeaders)"
    order_create_impl = "method:ts-order-service:order.service.OrderServiceImpl#create(order.entity.Order,org.springframework.http.HttpHeaders)"
    user_get_id_ctrl = "method:ts-user-service:user.controller.UserController#getUserByUserId(java.lang.String,org.springframework.http.HttpHeaders)"
    user_get_id_impl = "method:ts-user-service:user.service.impl.UserServiceImpl#findByUserId(java.lang.String,?HttpHeaders)"

    assurance_create_ctrl = "method:ts-assurance-service:assurance.controller.AssuranceController#createNewAssurance(int,java.lang.String,org.springframework.http.HttpHeaders)"
    assurance_create_impl = "method:ts-assurance-service:assurance.service.AssuranceServiceImpl#create(int,java.lang.String,org.springframework.http.HttpHeaders)"
    assurance_type_get_index = "method:ts-assurance-service:assurance.entity.AssuranceType#getTypeByIndex(int)"

    food_create_ctrl = "method:ts-food-service:foodsearch.controller.FoodController#createFoodOrder(foodsearch.entity.FoodOrder,org.springframework.http.HttpHeaders)"
    food_create_impl = "method:ts-food-service:foodsearch.service.FoodServiceImpl#createFoodOrder(foodsearch.entity.FoodOrder,org.springframework.http.HttpHeaders)"
    food_create_batch_impl = "method:ts-food-service:foodsearch.service.FoodServiceImpl#createFoodOrdersInBatch(java.util.List,org.springframework.http.HttpHeaders)"
    food_mq_send = "method:ts-food-service:foodsearch.mq.RabbitSend#send(java.lang.String)"
    delivery_mq_recv = "method:ts-delivery-service:delivery.mq.RabbitReceive#process(java.lang.String)"

    consign_insert_ctrl = "method:ts-consign-service:consign.controller.ConsignController#insertConsign(consign.entity.Consign,org.springframework.http.HttpHeaders)"
    consign_insert_impl = "method:ts-consign-service:consign.service.ConsignServiceImpl#insertConsignRecord(consign.entity.Consign,org.springframework.http.HttpHeaders)"
    consign_price_ctrl = "method:ts-consign-price-service:consignprice.controller.ConsignPriceController#getPriceByWeightAndRegion(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    consign_price_impl = "method:ts-consign-price-service:consignprice.service.ConsignPriceServiceImpl#getPriceByWeightAndRegion(double,boolean,org.springframework.http.HttpHeaders)"

    def add_booking_high_speed(b, food=0, assurance=0, consign_kg=0.0):
        b.add_call(preserve_ctrl, preserve_impl)
        b.add_call(preserve_impl, preserve_sec)
        b.add_call(preserve_impl, preserve_contacts)
        b.add_call(preserve_impl, preserve_trip_detail)
        b.add_call(preserve_impl, preserve_dispatch)
        b.add_call(preserve_impl, preserve_order)
        b.add_call(preserve_impl, preserve_user)

        b.add_remote(preserve_sec, sec_check_ctrl)
        b.add_call(sec_check_ctrl, sec_check_impl)
        b.add_data(sec_check_impl, "res:table:ts-security-service.security_config")

        b.add_remote(preserve_contacts, contacts_get_id_ctrl)
        b.add_call(contacts_get_id_ctrl, contacts_get_id_impl)
        b.add_data(contacts_get_id_impl, "res:table:ts-contacts-service.contacts")

        b.add_remote(preserve_trip_detail, travel_detail_ctrl)
        b.add_call(travel_detail_ctrl, travel_detail_impl)
        b.add_data(travel_detail_impl, "res:table:ts-travel-service.trip")

        # Basic query
        b.add_remote(preserve_impl, basic_travel_ctrl)
        b.add_call(basic_travel_ctrl, basic_travel_impl)
        b.add_call(basic_travel_impl, basic_route_get)
        b.add_call(basic_travel_impl, basic_train_get)
        b.add_call(basic_travel_impl, basic_price_get)
        b.add_remote(basic_route_get, route_get_ctrl)
        b.add_remote(basic_train_get, train_get_ctrl)
        b.add_remote(basic_price_get, price_query_ctrl)
        b.add_call(price_query_ctrl, price_query_impl)
        b.add_data(price_query_impl, "res:table:ts-price-service.price_config")

        # Dispatch seat
        b.add_remote(preserve_dispatch, seat_dist_ctrl)
        b.add_call(seat_dist_ctrl, seat_dist_impl)

        # Create order
        b.add_remote(preserve_order, order_create_ctrl)
        b.add_call(order_create_ctrl, order_create_impl)
        b.add_data(order_create_impl, "res:table:ts-order-service.orders")

        # Get user
        b.add_remote(preserve_user, user_get_id_ctrl)
        b.add_call(user_get_id_ctrl, user_get_id_impl)
        b.add_data(user_get_id_impl, "res:table:ts-user-service.user")

        if assurance > 0:
            b.add_call(preserve_impl, preserve_assurance)
            b.add_remote(preserve_assurance, assurance_create_ctrl)
            b.add_call(assurance_create_ctrl, assurance_create_impl)
            b.add_call(assurance_create_impl, assurance_type_get_index)
            b.add_data(assurance_create_impl, "res:table:ts-assurance-service.assurance")

        if food > 0:
            b.add_call(preserve_impl, preserve_food)
            b.add_remote(preserve_food, food_create_ctrl)
            b.add_call(food_create_ctrl, food_create_impl)
            b.add_call(food_create_impl, food_create_batch_impl)
            b.add_call(food_create_batch_impl, food_mq_send)
            b.add_data(food_create_batch_impl, "res:table:ts-food-service.food_order")
            b.add_remote(food_mq_send, delivery_mq_recv)
            b.add_data(delivery_mq_recv, "res:table:ts-delivery-service.delivery")

        if consign_kg > 0:
            b.add_call(preserve_impl, preserve_consign)
            b.add_remote(preserve_consign, consign_insert_ctrl)
            b.add_call(consign_insert_ctrl, consign_insert_impl)
            b.add_remote(consign_insert_impl, consign_price_ctrl)
            b.add_call(consign_price_ctrl, consign_price_impl)
            b.add_data(consign_price_impl, "res:table:ts-consign-price-service.consign_price")
            b.add_data(consign_insert_impl, "res:table:ts-consign-mysql.consign_record")

    # Booking Regular train: ts-preserve-other-service
    preserve_other_ctrl = "method:ts-preserve-other-service:preserveOther.controller.PreserveOtherController#preserve(edu.fudan.common.entity.OrderTicketsInfo,org.springframework.http.HttpHeaders)"
    preserve_other_impl = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#preserve(edu.fudan.common.entity.OrderTicketsInfo,org.springframework.http.HttpHeaders)"
    preserve_other_sec = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#checkSecurity(java.lang.String,org.springframework.http.HttpHeaders)"
    preserve_other_contacts = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#getContactsById(java.lang.String,org.springframework.http.HttpHeaders)"
    preserve_other_trip_detail = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#getTripAllDetailInformation(edu.fudan.common.entity.TripAllDetailInfo,org.springframework.http.HttpHeaders)"
    preserve_other_dispatch = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#dipatchSeat(java.lang.String,java.lang.String,java.lang.String,java.lang.String,int,int,java.util.List,org.springframework.http.HttpHeaders)"
    preserve_other_order = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#createOrder(edu.fudan.common.entity.Order,org.springframework.http.HttpHeaders)"
    preserve_other_user = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#getAccount(java.lang.String,org.springframework.http.HttpHeaders)"
    preserve_other_food = "method:ts-preserve-other-service:preserveOther.service.PreserveOtherServiceImpl#createFoodOrder(edu.fudan.common.entity.FoodOrder,org.springframework.http.HttpHeaders)"

    travel2_detail_ctrl = "method:ts-travel2-service:travel2.controller.Travel2Controller#getTripAllDetailInfo(edu.fudan.common.entity.TripAllDetailInfo,org.springframework.http.HttpHeaders)"
    travel2_detail_impl = "method:ts-travel2-service:travel2.service.TravelServiceImpl#getTripAllDetailInfo(edu.fudan.common.entity.TripAllDetailInfo,org.springframework.http.HttpHeaders)"
    order_other_create_ctrl = "method:ts-order-other-service:other.controller.OrderOtherController#createNewOrder(other.entity.Order,org.springframework.http.HttpHeaders)"
    order_other_create_impl = "method:ts-order-other-service:other.service.OrderOtherServiceImpl#create(other.entity.Order,org.springframework.http.HttpHeaders)"

    def add_booking_regular(b, food=0, assurance=0, consign_kg=0.0):
        b.add_call(preserve_other_ctrl, preserve_other_impl)
        b.add_call(preserve_other_impl, preserve_other_sec)
        b.add_call(preserve_other_impl, preserve_other_contacts)
        b.add_call(preserve_other_impl, preserve_other_trip_detail)
        b.add_call(preserve_other_impl, preserve_other_dispatch)
        b.add_call(preserve_other_impl, preserve_other_order)
        b.add_call(preserve_other_impl, preserve_other_user)

        b.add_remote(preserve_other_sec, sec_check_ctrl)
        b.add_call(sec_check_ctrl, sec_check_impl)
        b.add_data(sec_check_impl, "res:table:ts-security-service.security_config")

        b.add_remote(preserve_other_contacts, contacts_get_id_ctrl)
        b.add_call(contacts_get_id_ctrl, contacts_get_id_impl)
        b.add_data(contacts_get_id_impl, "res:table:ts-contacts-service.contacts")

        b.add_remote(preserve_other_trip_detail, travel2_detail_ctrl)
        b.add_call(travel2_detail_ctrl, travel2_detail_impl)
        b.add_data(travel2_detail_impl, "res:table:ts-travel2-service.trip2")

        # Basic query
        b.add_remote(preserve_other_impl, basic_travel_ctrl)
        b.add_call(basic_travel_ctrl, basic_travel_impl)
        b.add_call(basic_travel_impl, basic_route_get)
        b.add_call(basic_travel_impl, basic_train_get)
        b.add_call(basic_travel_impl, basic_price_get)
        b.add_remote(basic_route_get, route_get_ctrl)
        b.add_remote(basic_train_get, train_get_ctrl)
        b.add_remote(basic_price_get, price_query_ctrl)
        b.add_call(price_query_ctrl, price_query_impl)
        b.add_data(price_query_impl, "res:table:ts-price-service.price_config")

        # Dispatch seat
        b.add_remote(preserve_other_dispatch, seat_dist_ctrl)
        b.add_call(seat_dist_ctrl, seat_dist_impl)

        # Create order other
        b.add_remote(preserve_other_order, order_other_create_ctrl)
        b.add_call(order_other_create_ctrl, order_other_create_impl)
        b.add_data(order_other_create_impl, "res:table:ts-order-other-service.orders_other")

        # Get user
        b.add_remote(preserve_other_user, user_get_id_ctrl)
        b.add_call(user_get_id_ctrl, user_get_id_impl)
        b.add_data(user_get_id_impl, "res:table:ts-user-service.user")

        if food > 0:
            b.add_call(preserve_other_impl, preserve_other_food)
            b.add_remote(preserve_other_food, food_create_ctrl)
            b.add_call(food_create_ctrl, food_create_impl)
            b.add_call(food_create_impl, food_create_batch_impl)
            b.add_call(food_create_batch_impl, food_mq_send)
            b.add_data(food_create_batch_impl, "res:table:ts-food-service.food_order")
            b.add_remote(food_mq_send, delivery_mq_recv)
            b.add_data(delivery_mq_recv, "res:table:ts-delivery-service.delivery")

    # Payment: ts-inside-payment-service
    pay_ctrl = "method:ts-inside-payment-service:inside_payment.controller.InsidePaymentController#pay(inside_payment.entity.PaymentInfo,org.springframework.http.HttpHeaders)"
    pay_impl = "method:ts-inside-payment-service:inside_payment.service.InsidePaymentServiceImpl#pay(inside_payment.entity.PaymentInfo,org.springframework.http.HttpHeaders)"
    pay_set_status = "method:ts-inside-payment-service:inside_payment.service.InsidePaymentServiceImpl#setOrderStatus(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    order_modify_ctrl = "method:ts-order-service:order.controller.OrderController#modifyOrder(java.lang.String,int,org.springframework.http.HttpHeaders)"
    order_modify_impl = "method:ts-order-service:order.service.OrderServiceImpl#modifyOrder(java.lang.String,int,org.springframework.http.HttpHeaders)"
    order_other_modify_ctrl = "method:ts-order-other-service:other.controller.OrderOtherController#modifyOrder(java.lang.String,int,org.springframework.http.HttpHeaders)"
    order_other_modify_impl = "method:ts-order-other-service:other.service.OrderOtherServiceImpl#modifyOrder(java.lang.String,int,org.springframework.http.HttpHeaders)"
    order_get_id_ctrl = "method:ts-order-service:order.controller.OrderController#getOrderById(java.lang.String,org.springframework.http.HttpHeaders)"
    order_get_id_impl = "method:ts-order-service:order.service.OrderServiceImpl#getOrderById(java.lang.String,org.springframework.http.HttpHeaders)"
    order_other_get_id_ctrl = "method:ts-order-other-service:other.controller.OrderOtherController#getOrderById(java.lang.String,org.springframework.http.HttpHeaders)"
    order_other_get_id_impl = "method:ts-order-other-service:other.service.OrderOtherServiceImpl#getOrderById(java.lang.String,org.springframework.http.HttpHeaders)"

    # Outside payment: ts-payment-service
    ext_pay_ctrl = "method:ts-payment-service:com.trainticket.controller.PaymentController#pay(com.trainticket.entity.Payment,org.springframework.http.HttpHeaders)"
    ext_pay_impl = "method:ts-payment-service:com.trainticket.service.PaymentServiceImpl#pay(com.trainticket.entity.Payment,org.springframework.http.HttpHeaders)"

    def add_payment(b, high_speed=True, external=False):
        b.add_call(pay_ctrl, pay_impl)
        b.add_call(pay_impl, pay_set_status)
        b.add_data(pay_impl, "res:table:ts-inside-payment-service.inside_payment")
        b.add_data(pay_impl, "res:table:ts-inside-payment-service.inside_money")
        if high_speed:
            b.add_remote(pay_impl, order_get_id_ctrl)
            b.add_call(order_get_id_ctrl, order_get_id_impl)
            b.add_data(order_get_id_impl, "res:table:ts-order-service.orders")
            b.add_remote(pay_set_status, order_modify_ctrl)
            b.add_call(order_modify_ctrl, order_modify_impl)
            b.add_data(order_modify_impl, "res:table:ts-order-service.orders")
        else:
            b.add_remote(pay_impl, order_other_get_id_ctrl)
            b.add_call(order_other_get_id_ctrl, order_other_get_id_impl)
            b.add_data(order_other_get_id_impl, "res:table:ts-order-other-service.orders_other")
            b.add_remote(pay_set_status, order_other_modify_ctrl)
            b.add_call(order_other_modify_ctrl, order_other_modify_impl)
            b.add_data(order_other_modify_impl, "res:table:ts-order-other-service.orders_other")
        if external:
            b.add_remote(pay_impl, ext_pay_ctrl)
            b.add_call(ext_pay_ctrl, ext_pay_impl)
            b.add_data(ext_pay_impl, "res:table:ts-payment-service.payment")

    # Order details and price check
    order_price_ctrl = "method:ts-order-service:order.controller.OrderController#getOrderPrice(java.lang.String,org.springframework.http.HttpHeaders)"
    order_price_impl = "method:ts-order-service:order.service.OrderServiceImpl#getOrderPrice(java.lang.String,org.springframework.http.HttpHeaders)"

    def add_order_details(b, high_speed=True):
        if high_speed:
            b.add_call(order_get_id_ctrl, order_get_id_impl)
            b.add_data(order_get_id_impl, "res:table:ts-order-service.orders")
            b.add_call(order_price_ctrl, order_price_impl)
            b.add_data(order_price_impl, "res:table:ts-order-service.orders")

    # Collect ticket & Enter station: ts-execute-service
    execute_collect_ctrl = "method:ts-execute-service:execute.controller.ExecuteControlller#collectTicket(java.lang.String,org.springframework.http.HttpHeaders)"
    execute_collect_impl = "method:ts-execute-service:execute.serivce.ExecuteServiceImpl#ticketCollect(java.lang.String,org.springframework.http.HttpHeaders)"
    execute_board_ctrl = "method:ts-execute-service:execute.controller.ExecuteControlller#executeTicket(java.lang.String,org.springframework.http.HttpHeaders)"
    execute_board_impl = "method:ts-execute-service:execute.serivce.ExecuteServiceImpl#ticketExecute(java.lang.String,org.springframework.http.HttpHeaders)"
    execute_get_order = "method:ts-execute-service:execute.serivce.ExecuteServiceImpl#getOrderByIdFromOrder(java.lang.String,org.springframework.http.HttpHeaders)"
    execute_get_order_other = "method:ts-execute-service:execute.serivce.ExecuteServiceImpl#getOrderByIdFromOrderOther(java.lang.String,org.springframework.http.HttpHeaders)"

    def add_collect_and_board(b, high_speed=True):
        b.add_call(execute_collect_ctrl, execute_collect_impl)
        b.add_call(execute_collect_impl, execute_get_order)
        b.add_call(execute_board_ctrl, execute_board_impl)
        b.add_call(execute_board_impl, execute_get_order)
        if high_speed:
            b.add_remote(execute_get_order, order_get_id_ctrl)
            b.add_remote(execute_collect_impl, order_modify_ctrl)
            b.add_remote(execute_board_impl, order_modify_ctrl)
        else:
            b.add_call(execute_collect_impl, execute_get_order_other)
            b.add_call(execute_board_impl, execute_get_order_other)
            b.add_remote(execute_get_order, order_get_id_ctrl)
            b.add_remote(execute_get_order_other, order_other_get_id_ctrl)
            b.add_remote(execute_collect_impl, order_other_modify_ctrl)
            b.add_remote(execute_board_impl, order_other_modify_ctrl)

    # Cancel & Refund: ts-cancel-service
    cancel_calc_ctrl = "method:ts-cancel-service:cancel.controller.CancelController#calculate(java.lang.String,org.springframework.http.HttpHeaders)"
    cancel_calc_impl = "method:ts-cancel-service:cancel.service.CancelServiceImpl#calculateRefund(java.lang.String,org.springframework.http.HttpHeaders)"
    cancel_calc_order_impl = "method:ts-cancel-service:cancel.service.CancelServiceImpl#calculateRefund(edu.fudan.common.entity.Order)"
    cancel_order_ctrl = "method:ts-cancel-service:cancel.controller.CancelController#cancelTicket(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    cancel_order_impl = "method:ts-cancel-service:cancel.service.CancelServiceImpl#cancelOrder(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    cancel_from_order = "method:ts-cancel-service:cancel.service.CancelServiceImpl#cancelFromOrder(edu.fudan.common.entity.Order,org.springframework.http.HttpHeaders)"
    cancel_from_other_order = "method:ts-cancel-service:cancel.service.CancelServiceImpl#cancelFromOtherOrder(edu.fudan.common.entity.Order,org.springframework.http.HttpHeaders)"
    cancel_drawback = "method:ts-cancel-service:cancel.service.CancelServiceImpl#drawbackMoney(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    cancel_get_order = "method:ts-cancel-service:cancel.service.CancelServiceImpl#getOrderByIdFromOrder(java.lang.String,org.springframework.http.HttpHeaders)"
    cancel_get_order_other = "method:ts-cancel-service:cancel.service.CancelServiceImpl#getOrderByIdFromOrderOther(java.lang.String,org.springframework.http.HttpHeaders)"
    cancel_get_user = "method:ts-cancel-service:cancel.service.CancelServiceImpl#getAccount(java.lang.String,org.springframework.http.HttpHeaders)"

    order_save_ctrl = "method:ts-order-service:order.controller.OrderController#saveOrderInfo(order.entity.Order,org.springframework.http.HttpHeaders)"
    order_save_impl = "method:ts-order-service:order.service.OrderServiceImpl#saveChanges(order.entity.Order,org.springframework.http.HttpHeaders)"
    order_other_save_ctrl = "method:ts-order-other-service:other.controller.OrderOtherController#saveOrderInfo(other.entity.Order,org.springframework.http.HttpHeaders)"
    order_other_save_impl = "method:ts-order-other-service:other.service.OrderOtherServiceImpl#saveChanges(other.entity.Order,org.springframework.http.HttpHeaders)"
    inside_drawback_ctrl = "method:ts-inside-payment-service:inside_payment.controller.InsidePaymentController#drawBack(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    inside_drawback_impl = "method:ts-inside-payment-service:inside_payment.service.InsidePaymentServiceImpl#drawBack(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"

    def add_cancel_and_refund(b, high_speed=True, collected=False):
        b.add_call(cancel_calc_ctrl, cancel_calc_impl)
        b.add_call(cancel_calc_impl, cancel_get_order)
        b.add_call(cancel_order_ctrl, cancel_order_impl)
        b.add_call(cancel_order_impl, cancel_get_order)

        if high_speed:
            b.add_remote(cancel_get_order, order_get_id_ctrl)
            if not collected:
                b.add_call(cancel_calc_impl, cancel_calc_order_impl)
                b.add_call(cancel_order_impl, cancel_from_order)
                b.add_call(cancel_order_impl, cancel_calc_order_impl)
                b.add_call(cancel_order_impl, cancel_drawback)
                b.add_call(cancel_order_impl, cancel_get_user)
                b.add_remote(cancel_from_order, order_save_ctrl)
                b.add_call(order_save_ctrl, order_save_impl)
                b.add_data(order_save_impl, "res:table:ts-order-service.orders")
                b.add_remote(cancel_drawback, inside_drawback_ctrl)
                b.add_call(inside_drawback_ctrl, inside_drawback_impl)
                b.add_data(inside_drawback_impl, "res:table:ts-inside-payment-service.inside_money")
                b.add_data(inside_drawback_impl, "res:table:ts-inside-payment-service.inside_payment")
                b.add_remote(cancel_get_user, user_get_id_ctrl)
        else:
            b.add_call(cancel_calc_impl, cancel_get_order_other)
            b.add_call(cancel_order_impl, cancel_get_order_other)
            b.add_remote(cancel_get_order, order_get_id_ctrl)
            b.add_remote(cancel_get_order_other, order_other_get_id_ctrl)
            if not collected:
                b.add_call(cancel_calc_impl, cancel_calc_order_impl)
                b.add_call(cancel_order_impl, cancel_from_other_order)
                b.add_call(cancel_order_impl, cancel_calc_order_impl)
                b.add_call(cancel_order_impl, cancel_drawback)
                b.add_call(cancel_order_impl, cancel_get_user)
                b.add_remote(cancel_from_other_order, order_other_save_ctrl)
                b.add_call(order_other_save_ctrl, order_other_save_impl)
                b.add_data(order_other_save_impl, "res:table:ts-order-other-service.orders_other")
                b.add_remote(cancel_drawback, inside_drawback_ctrl)
                b.add_call(inside_drawback_ctrl, inside_drawback_impl)
                b.add_data(inside_drawback_impl, "res:table:ts-inside-payment-service.inside_money")
                b.add_data(inside_drawback_impl, "res:table:ts-inside-payment-service.inside_payment")
                b.add_remote(cancel_get_user, user_get_id_ctrl)

    # Rebook: ts-rebook-service
    rebook_ctrl = "method:ts-rebook-service:rebook.controller.RebookController#rebook(rebook.entity.RebookInfo,org.springframework.http.HttpHeaders)"
    rebook_impl = "method:ts-rebook-service:rebook.service.RebookServiceImpl#rebook(rebook.entity.RebookInfo,org.springframework.http.HttpHeaders)"
    rebook_get_order = "method:ts-rebook-service:rebook.service.RebookServiceImpl#getOrderByRebookInfo(rebook.entity.RebookInfo,org.springframework.http.HttpHeaders)"
    rebook_trip_detail = "method:ts-rebook-service:rebook.service.RebookServiceImpl#getTripAllDetailInformation(edu.fudan.common.entity.TripAllDetailInfo,java.lang.String,org.springframework.http.HttpHeaders)"
    rebook_dispatch = "method:ts-rebook-service:rebook.service.RebookServiceImpl#dipatchSeat(java.lang.String,java.lang.String,java.lang.String,java.lang.String,int,int,java.util.List,org.springframework.http.HttpHeaders)"
    rebook_drawback = "method:ts-rebook-service:rebook.service.RebookServiceImpl#drawBackMoney(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    rebook_update_order = "method:ts-rebook-service:rebook.service.RebookServiceImpl#updateOrder(edu.fudan.common.entity.Order,java.lang.String,org.springframework.http.HttpHeaders)"

    def add_rebook(b, high_speed=False, cheaper=True, collected=False):
        b.add_call(rebook_ctrl, rebook_impl)
        b.add_call(rebook_impl, rebook_get_order)
        if high_speed:
            b.add_remote(rebook_get_order, order_get_id_ctrl)
        else:
            b.add_remote(rebook_get_order, order_other_get_id_ctrl)

        if not collected:
            b.add_call(rebook_impl, rebook_trip_detail)
            b.add_call(rebook_impl, rebook_dispatch)
            b.add_call(rebook_impl, rebook_update_order)
            if not high_speed:
                b.add_remote(rebook_trip_detail, travel2_detail_ctrl)
                b.add_remote(rebook_update_order, order_other_save_ctrl)
            else:
                b.add_remote(rebook_trip_detail, travel_detail_ctrl)
                b.add_remote(rebook_update_order, order_save_ctrl)
            b.add_remote(rebook_dispatch, seat_dist_ctrl)
            if cheaper:
                b.add_call(rebook_impl, rebook_drawback)
                b.add_remote(rebook_drawback, inside_drawback_ctrl)

    # Consignment lookup
    consign_order_ctrl = "method:ts-consign-service:consign.controller.ConsignController#findByOrderId(java.lang.String,org.springframework.http.HttpHeaders)"
    consign_order_impl = "method:ts-consign-service:consign.service.ConsignServiceImpl#queryByOrderId(java.util.UUID,org.springframework.http.HttpHeaders)"
    assurance_order_ctrl = "method:ts-assurance-service:assurance.controller.AssuranceController#findAssuranceByOrderId(java.lang.String,org.springframework.http.HttpHeaders)"
    assurance_order_impl = "method:ts-assurance-service:assurance.service.AssuranceServiceImpl#findAssuranceByOrderId(java.util.UUID,org.springframework.http.HttpHeaders)"

    # Add Contact
    contact_create_ctrl = "method:ts-contacts-service:contacts.controller.ContactsController#createNewContacts(contacts.entity.Contacts,?HttpHeaders)"
    contact_create_impl = "method:ts-contacts-service:contacts.service.ContactsServiceImpl#createContacts(contacts.entity.Contacts,org.springframework.http.HttpHeaders)"

    # User registration
    user_reg_ctrl = "method:ts-user-service:user.controller.UserController#registerUser(user.dto.UserDto,org.springframework.http.HttpHeaders)"
    user_reg_impl = "method:ts-user-service:user.service.impl.UserServiceImpl#saveUser(user.dto.UserDto,?HttpHeaders)"
    user_reg_create_auth = "method:ts-user-service:user.service.impl.UserServiceImpl#createDefaultAuthUser(user.dto.AuthDto)"
    auth_reg_ctrl = "method:ts-auth-service:auth.controller.AuthController#createDefaultUser(auth.dto.AuthDto)"
    auth_reg_impl = "method:ts-auth-service:auth.service.impl.UserServiceImpl#createDefaultAuthUser(auth.dto.AuthDto)"

    # ==================== SCENARIO S1 ====================
    print("Building S1...")
    b1 = ScenarioBuilder("S1")
    add_common_login(b1)
    add_high_speed_search(b1)
    add_food_browse(b1)
    add_common_contacts(b1)
    add_booking_high_speed(b1, food=1, assurance=1, consign_kg=2.0)
    add_order_refresh(b1)
    add_order_details(b1, high_speed=True)
    add_payment(b1, high_speed=True, external=False)
    add_collect_and_board(b1, high_speed=True)
    # Consignment lookup of order
    b1.add_call(consign_order_ctrl, consign_order_impl)
    b1.add_data(consign_order_impl, "res:table:ts-consign-mysql.consign_record")
    # Assurance lookup of order
    b1.add_call(assurance_order_ctrl, assurance_order_impl)
    b1.add_data(assurance_order_impl, "res:table:ts-assurance-service.assurance")
    scenarios["S1"] = b1.build()

    # ==================== SCENARIO S2 ====================
    print("Building S2...")
    b2 = ScenarioBuilder("S2")
    add_common_login(b2)
    add_regular_search(b2)
    add_food_browse(b2)
    add_common_contacts(b2)
    add_booking_regular(b2, food=2, assurance=0, consign_kg=0.0)
    add_order_other_refresh(b2)
    add_payment(b2, high_speed=False, external=False)
    add_cancel_and_refund(b2, high_speed=False, collected=False)
    # 2nd booking: same regular train
    # add_booking_regular already adds the components
    # rebook to cheaper seat class
    add_rebook(b2, high_speed=False, cheaper=True, collected=False)
    scenarios["S2"] = b2.build()

    # ==================== SCENARIO S3 ====================
    print("Building S3...")
    b3 = ScenarioBuilder("S3")
    add_common_login(b3)
    add_high_speed_search(b3)
    add_food_browse(b3)
    add_common_contacts(b3)
    # Add new contact
    b3.add_call(contact_create_ctrl, contact_create_impl)
    b3.add_data(contact_create_impl, "res:table:ts-contacts-service.contacts")
    # Order 1: food, assurance, consign
    add_booking_high_speed(b3, food=2, assurance=1, consign_kg=0.5)
    # Order 2: no extras (already covered)
    add_order_refresh(b3)
    add_order_details(b3, high_speed=True)
    add_payment(b3, high_speed=True, external=False)
    # Check consign price
    b3.add_call(consign_price_ctrl, consign_price_impl)
    b3.add_data(consign_price_impl, "res:table:ts-consign-price-service.consign_price")
    # Look at assurance types
    assurance_types_ctrl = "method:ts-assurance-service:assurance.controller.AssuranceController#getAllAssuranceType(org.springframework.http.HttpHeaders)"
    assurance_types_impl = "method:ts-assurance-service:assurance.service.AssuranceServiceImpl#getAllAssuranceTypes(org.springframework.http.HttpHeaders)"
    b3.add_call(assurance_types_ctrl, assurance_types_impl)
    scenarios["S3"] = b3.build()

    # ==================== SCENARIO S4 ====================
    print("Building S4...")
    b4 = ScenarioBuilder("S4")
    # Register new user
    b4.add_call(user_reg_ctrl, user_reg_impl)
    b4.add_call(user_reg_impl, user_reg_create_auth)
    b4.add_remote(user_reg_create_auth, auth_reg_ctrl)
    b4.add_call(auth_reg_ctrl, auth_reg_impl)
    b4.add_data(user_reg_impl, "res:table:ts-user-service.user")
    b4.add_data(auth_reg_impl, "res:table:ts-auth-service.auth_user")
    b4.add_data(auth_reg_impl, "res:table:ts-auth-service.user_roles")
    # Login with new account
    add_common_login(b4)
    # Add contact
    b4.add_call(contact_create_ctrl, contact_create_impl)
    b4.add_data(contact_create_impl, "res:table:ts-contacts-service.contacts")
    # High-speed search
    add_high_speed_search(b4)
    add_food_browse(b4)
    add_booking_high_speed(b4, food=1, assurance=1, consign_kg=0.0)
    add_order_refresh(b4)
    add_order_details(b4, high_speed=True)
    add_payment(b4, high_speed=True, external=True)
    add_cancel_and_refund(b4, high_speed=True, collected=False)
    scenarios["S4"] = b4.build()

    # ==================== SCENARIO S5 ====================
    print("Building S5...")
    b5 = ScenarioBuilder("S5")
    add_common_login(b5)
    # TravelPlanController
    plan_cheapest_ctrl = "method:ts-travel-plan-service:travelplan.controller.TravelPlanController#getByCheapest(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    plan_quickest_ctrl = "method:ts-travel-plan-service:travelplan.controller.TravelPlanController#getByQuickest(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    plan_min_ctrl = "method:ts-travel-plan-service:travelplan.controller.TravelPlanController#getByMinStation(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    plan_transfer_ctrl = "method:ts-travel-plan-service:travelplan.controller.TravelPlanController#getTransferResult(travelplan.entity.TransferTravelInfo,org.springframework.http.HttpHeaders)"

    plan_cheapest_impl = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getCheapest(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    plan_quickest_impl = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getQuickest(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    plan_min_impl = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getMinStation(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    plan_transfer_impl = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getTransferSearch(travelplan.entity.TransferTravelInfo,org.springframework.http.HttpHeaders)"

    plan_res_cheapest = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getRoutePlanResultCheapest(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    plan_res_quickest = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getRoutePlanResultQuickest(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    plan_res_min = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getRoutePlanResultMinStation(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    plan_query_train = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#queryTrainTypeByName(java.lang.String,org.springframework.http.HttpHeaders)"
    plan_get_rest = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#getRestTicketNumber(java.lang.String,java.lang.String,java.lang.String,java.lang.String,int,int,java.util.List,org.springframework.http.HttpHeaders)"

    plan_trips_hs = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#tripsFromHighSpeed(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    plan_trips_normal = "method:ts-travel-plan-service:travelplan.service.TravelPlanServiceImpl#tripsFromNormal(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"

    rp_cheapest_ctrl = "method:ts-route-plan-service:plan.controller.RoutePlanController#getCheapestRoutes(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    rp_cheapest_impl = "method:ts-route-plan-service:plan.service.RoutePlanServiceImpl#searchCheapestResult(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    rp_quickest_ctrl = "method:ts-route-plan-service:plan.controller.RoutePlanController#getQuickestRoutes(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    rp_quickest_impl = "method:ts-route-plan-service:plan.service.RoutePlanServiceImpl#searchQuickestResult(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    rp_min_ctrl = "method:ts-route-plan-service:plan.controller.RoutePlanController#getMinStopStations(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"
    rp_min_impl = "method:ts-route-plan-service:plan.service.RoutePlanServiceImpl#searchMinStopStations(edu.fudan.common.entity.RoutePlanInfo,org.springframework.http.HttpHeaders)"

    rp_hs = "method:ts-route-plan-service:plan.service.RoutePlanServiceImpl#getTripFromHighSpeedTravelServive(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    rp_normal = "method:ts-route-plan-service:plan.service.RoutePlanServiceImpl#getTripFromNormalTrainTravelService(edu.fudan.common.entity.TripInfo,org.springframework.http.HttpHeaders)"
    route_by_stations_ctrl = "method:ts-route-service:route.controller.RouteController#queryByStartAndTerminal(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    route_by_stations_impl = "method:ts-route-service:route.service.RouteServiceImpl#getRouteByStartAndEnd(java.lang.String,java.lang.String,org.springframework.http.HttpHeaders)"
    travel_by_route_ctrl = "method:ts-travel-service:travel.controller.TravelController#getTripsByRouteId(java.util.ArrayList,org.springframework.http.HttpHeaders)"
    travel_by_route_impl = "method:ts-travel-service:travel.service.TravelServiceImpl#getTripByRoute(java.util.ArrayList,org.springframework.http.HttpHeaders)"
    travel2_by_route_ctrl = "method:ts-travel2-service:travel2.controller.Travel2Controller#getTripsByRouteId(java.util.ArrayList,org.springframework.http.HttpHeaders)"
    travel2_by_route_impl = "method:ts-travel2-service:travel2.service.TravelServiceImpl#getTripByRoute(java.util.ArrayList,org.springframework.http.HttpHeaders)"

    b5.add_call(plan_cheapest_ctrl, plan_cheapest_impl)
    b5.add_call(plan_cheapest_impl, plan_res_cheapest)
    b5.add_call(plan_cheapest_impl, plan_query_train)
    b5.add_call(plan_cheapest_impl, plan_get_rest)
    b5.add_remote(plan_res_cheapest, rp_cheapest_ctrl)
    b5.add_call(rp_cheapest_ctrl, rp_cheapest_impl)
    b5.add_call(rp_cheapest_impl, rp_hs)
    b5.add_call(rp_cheapest_impl, rp_normal)
    b5.add_remote(rp_hs, travel_query_ctrl)
    b5.add_remote(rp_normal, travel2_query_ctrl)

    b5.add_call(plan_quickest_ctrl, plan_quickest_impl)
    b5.add_call(plan_quickest_impl, plan_res_quickest)
    b5.add_call(plan_quickest_impl, plan_query_train)
    b5.add_call(plan_quickest_impl, plan_get_rest)
    b5.add_remote(plan_res_quickest, rp_quickest_ctrl)
    b5.add_call(rp_quickest_ctrl, rp_quickest_impl)
    b5.add_call(rp_quickest_impl, rp_hs)
    b5.add_call(rp_quickest_impl, rp_normal)

    b5.add_call(plan_min_ctrl, plan_min_impl)
    b5.add_call(plan_min_impl, plan_res_min)
    b5.add_call(plan_min_impl, plan_query_train)
    b5.add_call(plan_min_impl, plan_get_rest)
    b5.add_remote(plan_res_min, rp_min_ctrl)
    b5.add_call(rp_min_ctrl, rp_min_impl)
    b5.add_remote(rp_min_impl, route_by_stations_ctrl)
    b5.add_call(route_by_stations_ctrl, route_by_stations_impl)
    b5.add_data(route_by_stations_impl, "res:table:ts-route-service.route")
    b5.add_remote(rp_min_impl, travel_by_route_ctrl)
    b5.add_call(travel_by_route_ctrl, travel_by_route_impl)
    b5.add_data(travel_by_route_impl, "res:table:ts-travel-service.trip")
    b5.add_remote(rp_min_impl, travel2_by_route_ctrl)
    b5.add_call(travel2_by_route_ctrl, travel2_by_route_impl)
    b5.add_data(travel2_by_route_impl, "res:table:ts-travel2-service.trip2")

    b5.add_remote(plan_query_train, train_get_ctrl)
    b5.add_remote(plan_get_rest, seat_left_ctrl)

    b5.add_call(plan_transfer_ctrl, plan_transfer_impl)
    b5.add_call(plan_transfer_impl, plan_trips_hs)
    b5.add_call(plan_transfer_impl, plan_trips_normal)
    b5.add_remote(plan_trips_hs, travel_query_ctrl)
    b5.add_remote(plan_trips_normal, travel2_query_ctrl)

    add_high_speed_search(b5)
    add_regular_search(b5)
    add_food_browse(b5)
    add_common_contacts(b5)
    add_booking_regular(b5, food=0, assurance=0, consign_kg=0.0)
    add_order_other_refresh(b5)
    add_payment(b5, high_speed=False, external=False)
    add_collect_and_board(b5, high_speed=False)
    scenarios["S5"] = b5.build()

    # ==================== SCENARIO S6 ====================
    print("Building S6...")
    b6 = ScenarioBuilder("S6")
    add_common_login(b6)
    # Admin basic info
    admin_station_ctrl = "method:ts-admin-basic-info-service:adminbasic.controller.AdminBasicInfoController#addStation(edu.fudan.common.entity.Station,org.springframework.http.HttpHeaders)"
    admin_station_impl = "method:ts-admin-basic-info-service:adminbasic.service.AdminBasicInfoServiceImpl#addStation(edu.fudan.common.entity.Station,org.springframework.http.HttpHeaders)"
    station_create_ctrl = "method:ts-station-service:fdse.microservice.controller.StationController#create(fdse.microservice.entity.Station,org.springframework.http.HttpHeaders)"
    station_create_impl = "method:ts-station-service:fdse.microservice.service.StationServiceImpl#create(fdse.microservice.entity.Station,org.springframework.http.HttpHeaders)"

    admin_train_ctrl = "method:ts-admin-basic-info-service:adminbasic.controller.AdminBasicInfoController#addTrain(edu.fudan.common.entity.TrainType,org.springframework.http.HttpHeaders)"
    admin_train_impl = "method:ts-admin-basic-info-service:adminbasic.service.AdminBasicInfoServiceImpl#addTrain(edu.fudan.common.entity.TrainType,org.springframework.http.HttpHeaders)"
    train_create_ctrl = "method:ts-train-service:train.controller.TrainController#create(train.entity.TrainType,org.springframework.http.HttpHeaders)"
    train_create_impl = "method:ts-train-service:train.service.TrainServiceImpl#create(train.entity.TrainType,org.springframework.http.HttpHeaders)"

    admin_price_ctrl = "method:ts-admin-basic-info-service:adminbasic.controller.AdminBasicInfoController#addPrice(adminbasic.entity.PriceInfo,org.springframework.http.HttpHeaders)"
    admin_price_impl = "method:ts-admin-basic-info-service:adminbasic.service.AdminBasicInfoServiceImpl#addPrice(adminbasic.entity.PriceInfo,org.springframework.http.HttpHeaders)"
    price_create_ctrl = "method:ts-price-service:price.controller.PriceController#create(price.entity.PriceConfig,org.springframework.http.HttpHeaders)"
    price_create_impl = "method:ts-price-service:price.service.PriceServiceImpl#createNewPriceConfig(price.entity.PriceConfig,org.springframework.http.HttpHeaders)"

    admin_route_ctrl = "method:ts-admin-route-service:adminroute.controller.AdminRouteController#addRoute(edu.fudan.common.entity.RouteInfo,org.springframework.http.HttpHeaders)"
    admin_route_impl = "method:ts-admin-route-service:adminroute.service.AdminRouteServiceImpl#createAndModifyRoute(edu.fudan.common.entity.RouteInfo,?HttpHeaders)"
    admin_route_chk_stations = "method:ts-admin-route-service:adminroute.service.AdminRouteServiceImpl#checkStationsExists(java.util.List,?HttpHeaders)"
    station_batch_ctrl = "method:ts-station-service:fdse.microservice.controller.StationController#queryForIdBatch(java.util.List,org.springframework.http.HttpHeaders)"
    station_batch_impl = "method:ts-station-service:fdse.microservice.service.StationServiceImpl#queryForIdBatch(java.util.List,org.springframework.http.HttpHeaders)"
    route_create_ctrl = "method:ts-route-service:route.controller.RouteController#createAndModifyRoute(route.entity.RouteInfo,org.springframework.http.HttpHeaders)"
    route_create_impl = "method:ts-route-service:route.service.RouteServiceImpl#createAndModify(route.entity.RouteInfo,org.springframework.http.HttpHeaders)"

    admin_travel_ctrl = "method:ts-admin-travel-service:admintravel.controller.AdminTravelController#addTravel(edu.fudan.common.entity.TravelInfo,org.springframework.http.HttpHeaders)"
    admin_travel_impl = "method:ts-admin-travel-service:admintravel.service.AdminTravelServiceImpl#addTravel(edu.fudan.common.entity.TravelInfo,org.springframework.http.HttpHeaders)"
    admin_travel_chk = "method:ts-admin-travel-service:admintravel.service.AdminTravelServiceImpl#checkTravelInfo(edu.fudan.common.entity.TravelInfo,org.springframework.http.HttpHeaders)"
    admin_travel_chk_stations = "method:ts-admin-travel-service:admintravel.service.AdminTravelServiceImpl#checkStationsExists(java.util.List,org.springframework.http.HttpHeaders)"
    admin_travel_train = "method:ts-admin-travel-service:admintravel.service.AdminTravelServiceImpl#queryTrainTypeByName(java.lang.String,org.springframework.http.HttpHeaders)"
    admin_travel_route = "method:ts-admin-travel-service:admintravel.service.AdminTravelServiceImpl#getRouteByRouteId(java.lang.String,org.springframework.http.HttpHeaders)"
    travel_create_ctrl = "method:ts-travel-service:travel.controller.TravelController#createTrip(edu.fudan.common.entity.TravelInfo,org.springframework.http.HttpHeaders)"
    travel_create_impl = "method:ts-travel-service:travel.service.TravelServiceImpl#create(edu.fudan.common.entity.TravelInfo,org.springframework.http.HttpHeaders)"

    admin_order_ctrl = "method:ts-admin-order-service:adminorder.controller.AdminOrderController#getAllOrders(org.springframework.http.HttpHeaders)"
    admin_order_impl = "method:ts-admin-order-service:adminorder.service.AdminOrderServiceImpl#getAllOrders(org.springframework.http.HttpHeaders)"
    order_find_all_ctrl = "method:ts-order-service:order.controller.OrderController#findAllOrder(org.springframework.http.HttpHeaders)"
    order_find_all_impl = "method:ts-order-service:order.service.OrderServiceImpl#getAllOrders(org.springframework.http.HttpHeaders)"
    order_other_find_all_ctrl = "method:ts-order-other-service:other.controller.OrderOtherController#findAllOrder(org.springframework.http.HttpHeaders)"
    order_other_find_all_impl = "method:ts-order-other-service:other.service.OrderOtherServiceImpl#getAllOrders(org.springframework.http.HttpHeaders)"

    # Add station
    b6.add_call(admin_station_ctrl, admin_station_impl)
    b6.add_remote(admin_station_impl, station_create_ctrl)
    b6.add_call(station_create_ctrl, station_create_impl)
    b6.add_data(station_create_impl, "res:table:ts-station-service.station")

    # Add train
    b6.add_call(admin_train_ctrl, admin_train_impl)
    b6.add_remote(admin_train_impl, train_create_ctrl)
    b6.add_call(train_create_ctrl, train_create_impl)
    b6.add_data(train_create_impl, "res:table:ts-train-service.train_type")

    # Add price
    b6.add_call(admin_price_ctrl, admin_price_impl)
    b6.add_remote(admin_price_impl, price_create_ctrl)
    b6.add_call(price_create_ctrl, price_create_impl)
    b6.add_data(price_create_impl, "res:table:ts-price-service.price_config")

    # Add route
    b6.add_call(admin_route_ctrl, admin_route_impl)
    b6.add_call(admin_route_impl, admin_route_chk_stations)
    b6.add_remote(admin_route_chk_stations, station_batch_ctrl)
    b6.add_call(station_batch_ctrl, station_batch_impl)
    b6.add_data(station_batch_impl, "res:table:ts-station-service.station")
    b6.add_remote(admin_route_impl, route_create_ctrl)
    b6.add_call(route_create_ctrl, route_create_impl)
    b6.add_data(route_create_impl, "res:table:ts-route-service.route")
    b6.add_data(route_create_impl, "res:table:ts-route-service.route_distances")
    b6.add_data(route_create_impl, "res:table:ts-route-service.route_stations")

    # Add travel
    b6.add_call(admin_travel_ctrl, admin_travel_impl)
    b6.add_call(admin_travel_impl, admin_travel_chk)
    b6.add_call(admin_travel_chk, admin_travel_chk_stations)
    b6.add_remote(admin_travel_chk_stations, station_batch_ctrl)
    b6.add_call(admin_travel_chk, admin_travel_train)
    b6.add_remote(admin_travel_train, train_get_ctrl)
    b6.add_call(admin_travel_chk, admin_travel_route)
    b6.add_remote(admin_travel_route, route_get_ctrl)
    b6.add_remote(admin_travel_impl, travel_create_ctrl)
    b6.add_call(travel_create_ctrl, travel_create_impl)
    b6.add_data(travel_create_impl, "res:table:ts-travel-service.trip")

    # Passenger high-speed booking & payment
    add_high_speed_search(b6)
    add_food_browse(b6)
    add_common_contacts(b6)
    add_booking_high_speed(b6, food=0, assurance=0, consign_kg=0.0)
    add_order_refresh(b6)
    add_payment(b6, high_speed=True, external=False)

    # Admin view all orders
    b6.add_call(admin_order_ctrl, admin_order_impl)
    b6.add_remote(admin_order_impl, order_find_all_ctrl)
    b6.add_call(order_find_all_ctrl, order_find_all_impl)
    b6.add_data(order_find_all_impl, "res:table:ts-order-service.orders")
    b6.add_remote(admin_order_impl, order_other_find_all_ctrl)
    b6.add_call(order_other_find_all_ctrl, order_other_find_all_impl)
    b6.add_data(order_other_find_all_impl, "res:table:ts-order-other-service.orders_other")
    scenarios["S6"] = b6.build()

    # ==================== SCENARIO S7 ====================
    print("Building S7...")
    b7 = ScenarioBuilder("S7")
    add_common_login(b7)
    add_high_speed_search(b7)
    add_common_contacts(b7)
    add_booking_high_speed(b7, food=0, assurance=0, consign_kg=3.0)
    add_order_refresh(b7)
    add_payment(b7, high_speed=True, external=False)
    add_collect_and_board(b7, high_speed=True)

    # Refused cancel & refund
    add_cancel_and_refund(b7, high_speed=True, collected=True)
    # Refused rebook
    add_rebook(b7, high_speed=True, cheaper=False, collected=True)

    # Consignment lookup of order
    b7.add_call(consign_order_ctrl, consign_order_impl)
    b7.add_data(consign_order_impl, "res:table:ts-consign-mysql.consign_record")

    # Consignment update (weight unchanged -> no consign price call)
    consign_update_ctrl = "method:ts-consign-service:consign.controller.ConsignController#updateConsign(consign.entity.Consign,org.springframework.http.HttpHeaders)"
    consign_update_impl = "method:ts-consign-service:consign.service.ConsignServiceImpl#updateConsignRecord(consign.entity.Consign,org.springframework.http.HttpHeaders)"
    b7.add_call(consign_update_ctrl, consign_update_impl)
    b7.add_data(consign_update_impl, "res:table:ts-consign-mysql.consign_record")

    scenarios["S7"] = b7.build()

    # Final submission output
    submission = {
        "project": "trainticket",
        "scenarios": scenarios
    }

    print(f"Writing submission to {out_path}...")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(submission, f, indent=2, ensure_ascii=False)
    print("Done!")

if __name__ == "__main__":
    build_solution()
