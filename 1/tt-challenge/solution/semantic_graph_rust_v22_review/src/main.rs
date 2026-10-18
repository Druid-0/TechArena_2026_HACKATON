//! Static, scenario-guided analysis of the Train Ticket graph skeleton.
//! Entire index, dispatch analysis, context filtering, graph traversal,
//! and submission generation are implemented in Rust.
//! No Python program or example baseline is executed or imported.
//! Usage: cargo run --release -- --base /path/to/tt-challenge --out submission.json

use flate2::read::GzDecoder;
use serde_json::{json, Value};
use std::collections::{BTreeSet, HashMap, HashSet};
use std::env;
mod snapshot;
mod ledger;
mod resolve;
use snapshot::DbSnapshot;
use ledger::Ledger;
use std::error::Error;
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};

type Pair = (String, String);
type Edges = BTreeSet<Pair>;
type Adjacency = HashMap<String, HashSet<String>>;

fn read_json(path: &Path) -> Result<Value, Box<dyn Error>> {
    let file = File::open(path)?;
    let mut content = String::new();
    if path.extension().and_then(|s| s.to_str()) == Some("gz") {
        GzDecoder::new(file).read_to_string(&mut content)?;
    } else {
        let mut file = file;
        file.read_to_string(&mut content)?;
    }
    Ok(serde_json::from_str(&content)?)
}
fn str_prop<'a>(v: &'a Value, key: &str) -> &'a str {
    v.get(key).and_then(Value::as_str).unwrap_or("")
}

#[derive(Clone, Default)]
struct Node {
    kind: String,
    module: String,
    name: String,
    method: String,
    abstract_method: bool,
    deployment_units: Vec<String>,
}

struct Index {
    nodes: HashMap<String, Node>,
    calls: Adjacency,
    overrides: Adjacency,
    remote_calls: Adjacency,
    messages: Adjacency,
    dispatch: Adjacency,
    data_access: Adjacency,
    units: HashMap<String, HashSet<String>>,
    unit_sizes: HashMap<String, usize>,
}
fn add_adj(a: &mut Adjacency, from: &str, to: &str) {
    a.entry(from.to_owned()).or_default().insert(to.to_owned());
}
impl Index {
    fn new(skeleton: &Value) -> Result<Self, String> {
        let mut nodes = HashMap::new();
        let obj = skeleton["nodes"].as_object().ok_or("skeleton.nodes missing")?;
        for (id, value) in obj {
            let deployment_units = value.get("deployment_units").and_then(Value::as_array)
                .map(|x| x.iter().filter_map(Value::as_str).map(str::to_owned).collect())
                .unwrap_or_else(Vec::new);
            nodes.insert(id.clone(), Node {
                kind: str_prop(value,"kind").to_owned(),
                module: str_prop(value,"module").to_owned(),
                name: str_prop(value,"name").to_owned(),
                method: str_prop(value,"method").to_owned(),
                abstract_method: value.get("abstract").and_then(Value::as_bool).unwrap_or(false),
                deployment_units,
            });
        }
        let mut cs_method: HashMap<String, String> = HashMap::new();
        let mut units: HashMap<String, HashSet<String>> = HashMap::new();
        let mut size: HashMap<String, usize> = HashMap::new();
        for (id,node) in &nodes {
            if node.kind == "call_site" { cs_method.insert(id.clone(), node.method.clone()); }
            if node.kind == "module" {
                let unit_set: HashSet<String> = if node.deployment_units.is_empty() {
                    [node.name.clone()].into_iter().collect()
                } else { node.deployment_units.iter().cloned().collect() };
                for u in &unit_set { *size.entry(u.clone()).or_insert(0) += 1; }
                units.insert(node.name.clone(), unit_set);
            }
        }
        let mut ix = Index {
            nodes, calls: HashMap::new(), overrides: HashMap::new(),
            remote_calls: HashMap::new(), messages: HashMap::new(),
            dispatch: HashMap::new(), data_access: HashMap::new(),
            units, unit_sizes: size,
        };
        for e in skeleton["edges"].as_array().ok_or("skeleton.edges missing")? {
            let typ = str_prop(e,"type");
            let src = str_prop(e,"source");
            let dst = str_prop(e,"target");
            match typ {
                "calls" => {
                    if let Some(method) = cs_method.get(src) {
                        add_adj(&mut ix.calls, method, dst);
                    }
                },
                "overrides" => add_adj(&mut ix.overrides, dst, src),
                "remote_call" | "message_delivery" | "runtime_dispatch" | "data_access" => {
                    let caller = cs_method.get(src).map(String::as_str).unwrap_or(src);
                    let adj = match typ {
                        "remote_call" => &mut ix.remote_calls,
                        "message_delivery" => &mut ix.messages,
                        "runtime_dispatch" => &mut ix.dispatch,
                        _ => &mut ix.data_access,
                    };
                    add_adj(adj, caller, dst);
                },
                _ => {},
            }
        }
        Ok(ix)
    }

    fn method(&self, prefix: &str, required: bool) -> Result<Option<String>, String> {
        let needle = format!("method:{prefix}");
        let mut options: Vec<&String> = self.nodes.iter().filter_map(|(id,n)|
            if n.kind == "method" && id.starts_with(&needle) {Some(id)} else {None}
        ).collect();
        options.sort_by(|a,b| a.len().cmp(&b.len()).then_with(|| a.cmp(b)));
        match options.first() {
            Some(id) => Ok(Some((**id).clone())),
            None if !required => Ok(None),
            None => Err(format!("Method does not exist in skeleton: {prefix}")),
        }
    }
    fn unit_of(&self, method: &str) -> Result<String, String> {
        let n = self.nodes.get(method).ok_or(format!("Unknown method {method}"))?;
        let fallback: HashSet<String> = [n.module.clone()].into_iter().collect();
        let possibilities = self.units.get(&n.module).unwrap_or(&fallback);
        possibilities.iter().min_by(|a,b| {
            self.unit_sizes.get(*a).copied().unwrap_or(0)
                .cmp(&self.unit_sizes.get(*b).copied().unwrap_or(0))
                .then_with(|| a.cmp(b))
        }).cloned().ok_or(format!("No deployment unit for {method}"))
    }
    fn can_execute_in(&self, id: &str, unit: &str) -> bool {
        self.nodes.get(id).and_then(|n| self.units.get(&n.module))
            .map(|units| units.contains(unit)).unwrap_or(false)
    }
    fn cha(&self, method: &str) -> HashSet<String> {
        let mut discovered = HashSet::new();
        let mut todo = vec![method.to_owned()];
        while let Some(next) = todo.pop() {
            if let Some(implementations) = self.overrides.get(&next) {
                for m in implementations {
                    if discovered.insert(m.clone()) {todo.push(m.clone());}
                }
            }
        }
        discovered
    }
}

#[derive(Clone)]
struct Step {
    key: String,
    family: char,
    flags: String,
    status: String,
}
impl Step {
    fn new(key: &str, family: char, flags: &str, status: &str) -> Self {
        Self { key: key.to_owned(), family, flags: flags.to_owned(), status: status.to_owned() }
    }
    fn has(&self, flag: &str) -> bool { self.flags.split('|').any(|item| item == flag) }
    fn collected(&self) -> bool { self.status == "collected" }
}

fn contains_any(haystack: &str, needles: &[&str]) -> bool {
    needles.iter().any(|needle| haystack.contains(*needle))
}

// Path-sensitive analysis transferred from v6 without changing its branch predicates.
fn allowed_method_call(src: &str, dst: &str, s: &Step, unit: &str) -> bool {
    let typ = s.family;
    // Freshly registered S4 passenger has no earlier orders. The SQL lookup
    // in the security check happens, but its per-order date parsing loop is empty.
    if s.has("new_account") && src.contains("ServiceImpl#checkSecurityAboutOrder(")
        && contains_any(dst, &["StringUtils#String2Date(","OrderStatus#getCode("]) {return false;}
    if dst.contains("StringUtils#String2Date(")
        && contains_any(src, &["OrderServiceImpl#queryOrders(","OrderOtherServiceImpl#queryOrders("])
        && !s.has("order_filter") {return false;}
    if src.contains("OrderServiceImpl#queryOrders(") && dst.contains("OrderInfo#getState()")
        && !s.has("state_filter") {return false;}
    if s.has("empty_list") && src.contains("OrderOtherServiceImpl#queryOrders(")
        && contains_any(dst, &["StringUtils#String2Date(","QueryInfo#getState()", "other.entity.Order#<init>("]) {return false;}
    if src.contains("RebookServiceImpl#rebook(") && s.has("rebook_cheaper")
        && dst.contains("method:ts-common:edu.fudan.common.entity.Order#<init>()") {return false;}
    // A trip added earlier in the scenario starts with zero sold tickets.
    // During the first search and initial booking, OrderServiceImpl#getSoldTickets
    // returns its empty-list branch before constructing a Ticket or LeftTicketInfo.
    if s.has("new_trip") && src.contains("order.service.OrderServiceImpl#getSoldTickets(")
        && contains_any(dst, &[
            "edu.fudan.common.entity.Ticket#<init>(int,",
            "edu.fudan.common.entity.LeftTicketInfo#<init>()",
            "edu.fudan.common.entity.LeftTicketInfo#toString()",
        ]) {return false;}
    // A just-created trip cannot have sold seats before its first booking.
    // OrderServiceImpl#getSoldTickets returns Response(..., null) for G9101,
    // so SeatServiceImpl#distributeSeat never enters the guarded block that
    // invokes isContained(soldTickets, seat). Do not remove the unavoidable
    // Ticket() constructor or the order-service lookup.
    if s.has("new_trip")
        && src.contains("seat.service.SeatServiceImpl#distributeSeat(")
        && dst.contains("seat.service.SeatServiceImpl#isContained(") {return false;}
    // S2: the regular-order cancellation branch never fetches the user
    // for the high-speed-specific cancellation notification.
    if typ=='R' && src.contains("CancelServiceImpl#cancelOrder(")
        && contains_any(dst, &["CancelServiceImpl#getAccount(",
                               "edu.fudan.common.entity.NotifyInfo#<init>(",
                               "SeatClass#getNameByCode("]) {return false;}
    // S2: same-family rebooking updates the original order in place.
    // The delete/create paths execute only for cross-family switches.
    if s.has("rebook_cheaper") && src.contains("RebookServiceImpl#updateOrder(")
        && contains_any(dst, &["RebookServiceImpl#deleteOrder(",
                               "RebookServiceImpl#createOrder("]) {return false;}
    if s.has("consign_same_weight") && src.contains("ConsignServiceImpl#updateConsignRecord(")
        && contains_any(dst, &["ConsignServiceImpl#insertConsignRecord(", "ConsignServiceImpl#getServiceUrl("]) {return false;}
    if dst.to_lowercase().contains("verifycode") || dst.contains("UserOperationException#") {return false;}
    if contains_any(src, &["PreserveServiceImpl#preserve(","PreserveOtherServiceImpl#preserve("]) {
        if dst.contains("addAssuranceForOrder(") && !s.has("assurance") {return false;}
        if dst.contains("createFoodOrder(") && !s.has("food") {return false;}
        // FoodOrder() is in the same guarded branch as createFoodOrder().
        if dst.contains("edu.fudan.common.entity.FoodOrder#<init>(") && !s.has("food") {return false;}
        if dst.contains("createConsign(") && !s.has("consign") {return false;}
    }
    if src.contains("ExecuteServiceImpl#") && (typ=='H' || typ=='R') {
        if typ=='H' && contains_any(dst, &["getOrderByIdFromOrderOther(","executeOrderOther("]) {return false;}
        if typ=='R' && dst.contains("executeOrder(") {return false;}
    }
    if src.contains("CancelServiceImpl#") && (typ=='H'||typ=='R') {
        if typ=='H' && dst.contains("getOrderByIdFromOrderOther(") {return false;}
        if typ=='H' && dst.contains("cancelFromOtherOrder(") {return false;}
        if typ=='R' && dst.contains("cancelFromOrder(") {return false;}
        // After collection, cancelOrder returns immediately following the status check;
        // high-speed notification constructors are never executed.
        if s.collected() && src.contains("CancelServiceImpl#cancelOrder(")
            && contains_any(dst, &["edu.fudan.common.entity.NotifyInfo#<init>(",
                                     "SeatClass#getNameByCode("]) {return false;}
        if s.collected() && contains_any(dst, &["cancelFromOrder(","cancelFromOtherOrder(",
            "drawbackMoney(","getAccount(","calculateRefund("]) {return false;}
    }
    if src.contains("RebookServiceImpl#rebook(") && s.collected()
        && !dst.contains("getOrderByRebookInfo(") {return false;}
    if typ=='H' && dst.contains("ts-order-other-service:") && unit!="ts-order-other-service" {return false;}
    if typ=='R' && dst.contains("ts-order-service:") && unit!="ts-order-service" {return false;}
    if typ=='H' && dst.contains("ts-travel2-service:") && unit!="ts-travel2-service" {return false;}
    if typ=='R' && dst.contains("ts-travel-service:") && unit!="ts-travel-service" {return false;}
    if typ=='H' && dst.contains("ts-preserve-other-service:") && unit!="ts-preserve-other-service" {return false;}
    if typ=='R' && dst.contains("ts-preserve-service:") && unit!="ts-preserve-service" {return false;}
    true
}

fn allowed_remote(src: &str, dst: &str, s: &Step) -> bool {
    let typ = s.family;
    // URL-aware dispatch: addAssuranceForOrder sends GET
    // /api/v1/assuranceservice/assurances/{typeIndex}/{orderId}.
    // Only AssuranceController#createNewAssurance maps to this URL.
    // AssuranceController#getAssuranceById maps to
    // /assurances/assuranceid/{assuranceId} and must not execute.
    if src.contains("#addAssuranceForOrder(")
        && dst.contains("AssuranceController#getAssuranceById(") {return false;}

    if s.has("consign_same_weight") && src.contains("ConsignServiceImpl#updateConsignRecord(")
        && dst.contains("ts-consign-price-service:") {return false;}
    if dst.contains("#home(") &&
        ((src.contains("getContactsById(") && dst.contains("ContactsController#home(")) ||
         (src.contains("FoodServiceImpl#getAllFood(") && dst.contains("TrainFoodController#home("))) {return false;}
    if dst.to_lowercase().contains("verifycode") {return false;}
    let cancel_high_probe = typ=='R' && src.contains("CancelServiceImpl#getOrderByIdFromOrder(")
        && dst.contains("method:ts-order-service:");
    let execute_high_probe = typ=='R' && src.contains("ExecuteServiceImpl#getOrderByIdFromOrder(")
        && dst.contains("method:ts-order-service:");
    let regular_high_probe = cancel_high_probe || execute_high_probe;
    // SecurityServiceImpl#check always queries BOTH order families regardless of
    // which train is being booked. Do not apply the train-family restriction.
    let security_other_probe = src.contains("SecurityServiceImpl#getSecurityOrderOtherInfoFromOrder(")
        && dst.contains("method:ts-order-other-service:");
    let security_high_probe = src.contains("SecurityServiceImpl#getSecurityOrderInfoFromOrder(")
        && dst.contains("method:ts-order-service:");
    let food_route_probe = typ=='R' && src.contains("FoodServiceImpl#getAllFood(")
        && dst.contains("method:ts-travel-service:");
    if typ=='R' && src.contains("FoodServiceImpl#getAllFood(")
        && dst.contains("method:ts-station-food-service:") {return false;}
    if typ=='H' {
        if dst.contains("method:ts-order-other-service:") && !security_other_probe {return false;}
        if contains_any(dst, &["method:ts-travel2-service:","method:ts-preserve-other-service:"]) {return false;}
    } else if typ=='R' {
        if dst.contains("method:ts-preserve-service:") {return false;}
        if dst.contains("method:ts-travel-service:") && !food_route_probe {return false;}
        if dst.contains("method:ts-order-service:") && !regular_high_probe && !security_high_probe {return false;}
    }
    if dst.contains("ts-payment-service:") && !s.has("external_payment") {return false;}
    if contains_any(src, &["InsidePaymentServiceImpl#","CancelServiceImpl#","RebookServiceImpl#"]) {
        if typ=='H' && dst.contains("ts-order-other-service:") {return false;}
        if typ=='R' && dst.contains("ts-order-service:") && !cancel_high_probe {return false;}
    }
    if src.contains("AdminTravelServiceImpl#addTravel(") {
        if typ=='H' && dst.contains("ts-travel2-service:") {return false;}
        if typ=='R' && dst.contains("ts-travel-service:") {return false;}
    }
    if src.contains("PreserveServiceImpl#getTripAllDetailInformation(") {
        if typ=='H' && dst.contains("ts-travel2-service:") {return false;}
        if typ=='R' && dst.contains("ts-travel-service:") {return false;}
    }
    if src.contains("PreserveOtherServiceImpl#getTripAllDetailInformation(") {
        if typ=='H' && dst.contains("ts-travel2-service:") {return false;}
        if typ=='R' && dst.contains("ts-travel-service:") {return false;}
    }
    if s.collected() {
        if src.contains("CancelServiceImpl#") && contains_any(dst, &["ts-inside-payment-service:","ts-notification-service:"]) {return false;}
        if src.contains("RebookServiceImpl#") {
            let is_order_lookup = src.contains("RebookServiceImpl#getOrderByRebookInfo(");
            let mut blocked = vec!["ts-seat-service:", "ts-inside-payment-service:"];
            if !is_order_lookup {blocked.extend(["ts-order-service:", "ts-order-other-service:"]);}
            if contains_any(dst, &blocked) {return false;}
        }
    }
    true
}

#[derive(Default)]
struct Graph {
    methods: BTreeSet<String>,
    calls: Edges,
    remote: Edges,
    data: Edges,
}
impl Graph {
    fn merge(&mut self, other: Graph) {
        self.methods.extend(other.methods);
        self.calls.extend(other.calls);
        self.remote.extend(other.remote);
        self.data.extend(other.data);
    }
    fn render(&self, sid: &str) -> Value {
        let mut edges = Vec::new();
        for m in &self.methods { edges.push(json!({"source":format!("scenario:{sid}"),"relation":"executes","target":m})); }
        for (a,b) in &self.calls { edges.push(json!({"source":a,"relation":"calls","target":b})); }
        for (a,b) in &self.remote { edges.push(json!({"source":a,"relation":"remote","target":b})); }
        for (a,b) in &self.data { edges.push(json!({"source":a,"relation":"data","target":b})); }
        json!({"edges":edges})
    }
}
fn guided_closure(ix: &Index, entry: &str, s: &Step) -> Result<Graph, String> {
    let mut seen: HashSet<(String,String)> = HashSet::new();
    let mut stack = vec![(entry.to_owned(), ix.unit_of(entry)?)];
    let mut result = Graph::default();
    while let Some((method, unit)) = stack.pop() {
        if !seen.insert((method.clone(), unit.clone())) {continue;}
        result.methods.insert(method.clone());
        if let Some(targets) = ix.calls.get(&method) {
            for target in targets {
                let mut possibilities = ix.cha(target);
                possibilities.insert(target.clone());
                if let Some(dispatch) = ix.dispatch.get(&method) {
                    possibilities.extend(dispatch.iter().cloned());
                }
                for x in possibilities {
                    if ix.nodes.get(&x).map(|n|n.abstract_method).unwrap_or(false) {continue;}
                    if !ix.can_execute_in(&x, &unit) {continue;}
                    if !allowed_method_call(&method, &x, s, &unit) {continue;}
                    result.calls.insert((method.clone(),x.clone()));
                    stack.push((x,unit.clone()));
                }
            }
        }
        let mut remote_candidates = HashSet::new();
        if let Some(v) = ix.remote_calls.get(&method) {remote_candidates.extend(v.iter().cloned());}
        if let Some(v) = ix.messages.get(&method) {remote_candidates.extend(v.iter().cloned());}
        for x in remote_candidates {
            if !allowed_remote(&method, &x, s) {continue;}
            result.remote.insert((method.clone(),x.clone()));
            stack.push((x.clone(), ix.unit_of(&x)?));
        }
        if let Some(tables) = ix.data_access.get(&method) {
            for t in tables { result.data.insert((method.clone(),t.clone())); }
        }
    }
    Ok(result)
}

fn append_method(ix:&Index, methods:&mut BTreeSet<String>, prefix:&str) -> Result<(),String> {
    if let Some(m) = ix.method(prefix,false)? { methods.insert(m); }
    Ok(())
}
fn add_call(ix:&Index, g:&mut Graph, a:&str, b:&str) -> Result<(),String> {
    if let (Some(x), Some(y)) = (ix.method(a,true)?,ix.method(b,true)?) {
        g.calls.insert((x,y));
    }
    Ok(())
}

fn build_block(ix: &Index, root_ids:&HashMap<&str,String>, sid: &str, steps:&[Step]) -> Result<Value,String> {
    let mut out = Graph::default();
    for s in steps {
        let entry = root_ids.get(s.key.as_str()).ok_or(format!("No root {}",s.key))?;
        out.merge(guided_closure(ix, entry, s)?);
    }
    let keys: HashSet<&str> = steps.iter().map(|s|s.key.as_str()).collect();
    if keys.contains("login") {
        for prefix in [
            "ts-auth-service:auth.entity.User#getAuthorities(",
            "ts-auth-service:auth.entity.User#getPassword(",
            "ts-auth-service:auth.entity.User#getUsername(",
            "ts-auth-service:auth.entity.User#isAccountNonExpired(",
            "ts-auth-service:auth.entity.User#isAccountNonLocked(",
            "ts-auth-service:auth.entity.User#isCredentialsNonExpired(",
            "ts-auth-service:auth.entity.User#isEnabled(",
            "ts-auth-service:auth.security.UserDetailsServiceImpl#loadUserByUsername(",
            "ts-common:edu.fudan.common.security.jwt.JWTFilter#doFilterInternal(",
        ] { append_method(ix,&mut out.methods,prefix)?; }
        add_call(ix,&mut out,
            "ts-auth-service:auth.service.impl.TokenServiceImpl#getToken(",
            "ts-auth-service:auth.entity.User#getPassword(")?;
        add_call(ix,&mut out,
            "ts-auth-service:auth.security.jwt.JWTProvider#createToken(",
            "ts-auth-service:auth.entity.User#getUsername(")?;
        if let Some(load) = ix.method("ts-auth-service:auth.security.UserDetailsServiceImpl#loadUserByUsername(",false)? {
            let table = "res:table:ts-auth-service.auth_user";
            if ix.nodes.get(table).map(|n|n.kind=="data_resource").unwrap_or(false) {
                out.data.insert((load,table.to_owned()));
            }
        }
    }
    let high = steps.iter().any(|s|s.key=="orders_high" && !s.has("empty_list"));
    let regular = steps.iter().any(|s|s.key=="orders_regular" && !s.has("empty_list"));
    if high || regular {
        let high_ctor = if high { ix.method("ts-order-service:order.entity.Order#<init>(",false)? } else {None};
        let regular_ctor = if regular {ix.method("ts-order-other-service:other.entity.Order#<init>(",false)?} else {None};
        let status = ix.method("ts-common:edu.fudan.common.entity.OrderStatus#getCode(",true)?;
        let seat = ix.method("ts-common:edu.fudan.common.entity.SeatClass#getCode(",true)?;
        let date = ix.method("ts-common:edu.fudan.common.util.StringUtils#Date2String(",true)?;
        for m in [&high_ctor, &regular_ctor, &status, &seat, &date].into_iter().flatten() {
            out.methods.insert(m.clone());
        }
        for ctor in [&high_ctor, &regular_ctor].into_iter().flatten() {
            for target in [&status,&seat,&date].into_iter().flatten() {
                out.calls.insert((ctor.clone(),target.clone()));
            }
        }
    }
    let filter_high = steps.iter().any(|s|s.key=="orders_high" && s.has("order_filter"));
    // Travel/purchase-date refresh in S7 applies the filter to the active
    // high-speed orders, not the old Other orders; S0 corroborates this pattern.
    let filter_reg = steps.iter().any(|s|s.key=="orders_regular" && s.has("order_filter") && !s.has("empty_list"));
    if filter_high || filter_reg {
        let str2date = ix.method("ts-common:edu.fudan.common.util.StringUtils#String2Date(",true)?
            .ok_or("no String2Date method")?;
        if filter_high {
            let src = ix.method("ts-order-service:order.service.OrderServiceImpl#queryOrders(",true)?.ok_or("no high query")?;
            out.calls.insert((src,str2date.clone()));out.methods.insert(str2date.clone());
        }
        if filter_reg {
            let src = ix.method("ts-order-other-service:other.service.OrderOtherServiceImpl#queryOrders(",true)?.ok_or("no regular query")?;
            out.calls.insert((src,str2date.clone()));out.methods.insert(str2date.clone());
        }
    }
    Ok(out.render(sid))
}

const ROOT_PREFIXES: &[(&str,&str)] = &[
    ("login", "ts-auth-service:auth.controller.UserController#getToken("),
    ("register", "ts-user-service:user.controller.UserController#registerUser("),
    ("contact_list", "ts-contacts-service:contacts.controller.ContactsController#findContactsByAccountId("),
    ("contact_add", "ts-contacts-service:contacts.controller.ContactsController#createNewContacts("),
    ("high_search", "ts-travel-service:travel.controller.TravelController#queryInfo("),
    ("regular_search", "ts-travel2-service:travel2.controller.Travel2Controller#queryInfo("),
    ("train_food_menu", "ts-food-service:foodsearch.controller.FoodController#getAllFood("),
    ("station_food_stores", "ts-station-food-service:food.controller.StationFoodController#getFoodStoresOfStation("),
    ("station_food_list", "ts-station-food-service:food.controller.StationFoodController#getFoodListByStationFoodStoreId("),
    ("assurance_types", "ts-assurance-service:assurance.controller.AssuranceController#getAllAssuranceType("),
    ("book_high", "ts-preserve-service:preserve.controller.PreserveController#preserve("),
    ("book_regular", "ts-preserve-other-service:preserveOther.controller.PreserveOtherController#preserve("),
    ("orders_high", "ts-order-service:order.controller.OrderController#queryOrdersForRefresh("),
    ("orders_regular", "ts-order-other-service:other.controller.OrderOtherController#queryOrdersForRefresh("),
    ("order_get_high", "ts-order-service:order.controller.OrderController#getOrderById("),
    ("order_get_regular", "ts-order-other-service:other.controller.OrderOtherController#getOrderById("),
    ("order_price_high", "ts-order-service:order.controller.OrderController#getOrderPrice("),
    ("order_price_regular", "ts-order-other-service:other.controller.OrderOtherController#getOrderPrice("),
    ("pay_inside", "ts-inside-payment-service:inside_payment.controller.InsidePaymentController#pay("),
    ("collect", "ts-execute-service:execute.controller.ExecuteControlller#collectTicket("),
    ("enter_station", "ts-execute-service:execute.controller.ExecuteControlller#executeTicket("),
    ("consign_account", "ts-consign-service:consign.controller.ConsignController#findByAccountId("),
    ("consign_order", "ts-consign-service:consign.controller.ConsignController#findByOrderId("),
    ("consign_update", "ts-consign-service:consign.controller.ConsignController#updateConsign("),
    ("assurance_order", "ts-assurance-service:assurance.controller.AssuranceController#findAssuranceByOrderId("),
    ("refund_calc", "ts-cancel-service:cancel.controller.CancelController#calculate("),
    ("cancel", "ts-cancel-service:cancel.controller.CancelController#cancelTicket("),
    ("rebook", "ts-rebook-service:rebook.controller.RebookController#rebook("),
    ("consign_price", "ts-consign-price-service:consignprice.controller.ConsignPriceController#getPriceByWeightAndRegion("),
    ("adv_cheapest", "ts-travel-plan-service:travelplan.controller.TravelPlanController#getByCheapest("),
    ("adv_quickest", "ts-travel-plan-service:travelplan.controller.TravelPlanController#getByQuickest("),
    ("adv_stops", "ts-travel-plan-service:travelplan.controller.TravelPlanController#getByMinStation("),
    ("adv_transfer", "ts-travel-plan-service:travelplan.controller.TravelPlanController#getTransferResult("),
    ("admin_station_add", "ts-admin-basic-info-service:adminbasic.controller.AdminBasicInfoController#addStation("),
    ("admin_train_add", "ts-admin-basic-info-service:adminbasic.controller.AdminBasicInfoController#addTrain("),
    ("admin_price_add", "ts-admin-basic-info-service:adminbasic.controller.AdminBasicInfoController#addPrice("),
    ("admin_route_add", "ts-admin-route-service:adminroute.controller.AdminRouteController#addRoute("),
    ("admin_trip_add", "ts-admin-travel-service:admintravel.controller.AdminTravelController#addTravel("),
    ("admin_orders", "ts-admin-order-service:adminorder.controller.AdminOrderController#getAllOrders("),
];

// Scenario interpretation reads the supplied English scenario steps rather than
// a hand-coded S1-S7 schedule. Each recognized business action maps to one
// exposed controller entry; order-list refreshes map to two REST endpoints
// as in the shipped UI (client_order_list.js). Unknown actions are errors,
// except "open trip details": the supplied client JS does not independently
// issue a getTripAllDetailInfo request on that click.
fn parse_scenario(doc: &Value, db: &DbSnapshot) -> Result<Vec<Step>, String> {
    let rows = doc["steps"].as_array().ok_or("scenario.steps must be an array")?;
    let mut result = Vec::new();
    let mut family = 'B';
    let mut ledger = Ledger::new(db);
    let mut registered = false;
    let mut created_high_speed_trip = false;
    for (i, row) in rows.iter().enumerate() {
        let action = row["action"].as_str().ok_or(format!("step {i} missing action"))?;
        let a = action.to_lowercase();
        let has = |x: &str| a.contains(x);
        let starts = |x: &str| a.starts_with(x);
        if starts("register a new passenger") {
            result.push(Step::new("register", 'B', "", ""));
            registered = true;
            ledger.register();
        } else if has("log in") {
            result.push(Step::new("login", 'B', "", ""));
        } else if starts("search high-speed") {
            family = 'H';
            result.push(Step::new("high_search", family,
                if created_high_speed_trip {"new_trip"} else {""}, ""));
        } else if starts("search regular") {
            family = 'R';
            result.push(Step::new("regular_search", family, "", ""));
        } else if has("advanced search") {
            let k = if has("cheapest") {"adv_cheapest"}
                else if has("quickest") {"adv_quickest"}
                else if has("fewest stops") {"adv_stops"}
                else {return Err(format!("Unknown advanced-search action: {action}"));};
            result.push(Step::new(k, 'B', "", ""));
        } else if has("trips with a transfer") {
            result.push(Step::new("adv_transfer", 'B', "", ""));
        } else if starts("open the details") {
            if has("regular") || has("other") {family = 'R';}
            // The frontend click stores a trip selection but performs no extra GET.
        } else if starts("view the food menu") {
            result.push(Step::new("train_food_menu", family, "", ""));
        } else if starts("browse the food stores") {
            // Spring MVC binding cannot satisfy @PathVariable stationName
            // from the controller template {stationId}. The gateway request
            // selects a handler but argument resolution fails before the Java
            // method is invoked. Thus no method, call, or DB access occurs.
            // This is an isolated hypothesis for the station-store browse
            // steps, not an unconditional removal of station-food service.
        } else if starts("open the contact book") {
            result.push(Step::new("contact_list", 'B', "", ""));
        } else if starts("add a contact") || starts("add a new contact") {
            result.push(Step::new("contact_add", 'B', "", ""));
        } else if starts("add a new station") {
            result.push(Step::new("admin_station_add", 'B', "", ""));
        } else if starts("add a new train type") {
            result.push(Step::new("admin_train_add", 'B', "", ""));
        } else if starts("add a new route") {
            result.push(Step::new("admin_route_add", 'B', "", ""));
        } else if starts("add a price configuration") {
            result.push(Step::new("admin_price_add", 'B', "", ""));
        } else if starts("add a new high-speed trip") {
            result.push(Step::new("admin_trip_add", 'H', "", ""));
            created_high_speed_trip = true;
        } else if has("administrator") && has("all orders") {
            result.push(Step::new("admin_orders", 'B', "", ""));
        } else if starts("book ") {
            if has("regular") || has("'other'") {family = 'R';}
            if family != 'R' {family = 'H';}
            let mut flags = Vec::new();
            if has("food") && !has("without food") && !has("no food") {
                flags.push("food");
            }
            if has("assurance (insurance) of the first type") {
                flags.push("assurance");
            }
            if (has("consignment") || has("luggage"))
                && !has("without consignment") && !has("no luggage consignment")
                && !has("without food, assurance or consignment") {
                flags.push("consign");
            }
            if registered {flags.push("new_account");}
            if created_high_speed_trip && family == 'H' {flags.push("new_trip");}
            result.push(Step::new(if family=='R' {"book_regular"} else {"book_high"},
                family, &flags.join("|"), ""));
            ledger.book(family);
        } else if starts("refresh the list of orders") {
            let mut flags = Vec::new();
            if has("filtered") || has("filter '") {
                flags.push("order_filter");
                if has("status") {flags.push("state_filter");}
            }
            // S0 explicitly distinguishes the two manual refresh requests.
            // Unqualified refresh in the web UI submits both requests.
            let high_only = has("for high-speed");
            let regular_only = has("for regular");
            if !regular_only {
                result.push(Step::new("orders_high", 'H', &flags.join("|"), ""));
            }
            if !high_only {
                // The unqualified date/status refresh applies filtering to the
                // active high-speed list. The Other list still refreshes, but
                // does not execute the date/state predicate.
                let mut regular_flags = if !high_only && !regular_only && flags.contains(&"order_filter") {
                    Vec::new()
                } else { flags.clone() };
                if registered || !ledger.has_orders('R') { regular_flags.push("empty_list"); }
                result.push(Step::new("orders_regular", 'R', &regular_flags.join("|"), ""));
            }
        } else if starts("open the") && has("order") {
            result.push(Step::new(if family=='R' {"order_get_regular"} else {"order_get_high"},
                family, "", ""));
        } else if starts("check the price of the order") {
            result.push(Step::new(if family=='R' {"order_price_regular"} else {"order_price_high"},
                family, "", ""));
        } else if has("pay for") && has("order") {
            result.push(Step::new("pay_inside", family,
                if has("external payment") {"external_payment"} else {""}, ""));
            ledger.pay();
        } else if starts("collect the ticket") {
            result.push(Step::new("collect",family,"",""));
            ledger.collect();
        } else if starts("enter the station") {
            result.push(Step::new("enter_station",family,"",""));
        } else if starts("ask how much would be refunded") {
            result.push(Step::new("refund_calc", family,"",
                if ledger.is_collected() {"collected"} else {""}));
        } else if starts("cancel") || starts("try to cancel") {
            result.push(Step::new("cancel", family,"",
                if ledger.is_collected() {"collected"} else {""}));
            ledger.cancel();
        } else if starts("rebook") || starts("try to rebook") {
            result.push(Step::new("rebook",family,
                if has("cheaper") {"rebook_cheaper"} else {""},
                if ledger.is_collected() {"collected"} else {""}));
            ledger.rebook_with_refund(has("cheaper"));
        } else if starts("look up the luggage consignment") {
            result.push(Step::new(if has("account") {"consign_account"} else {"consign_order"},
                family,"",""));
        } else if starts("look up the assurance") {
            result.push(Step::new("assurance_order",family,"",""));
        } else if starts("change the consignee") {
            result.push(Step::new("consign_update",family,"consign_same_weight",""));
        } else if has("consignment within the region costs") {
            result.push(Step::new("consign_price",family,"",""));
        } else if has("list of assurance types") {
            result.push(Step::new("assurance_types",family,"",""));
        } else {
            return Err(format!("Unsupported scenario action at step {}: {}",i+1,action));
        }
    }
    Ok(result)
}

fn main() -> Result<(), Box<dyn Error>> {
    let mut base: Option<PathBuf> = None;
    let mut out = PathBuf::from("submission.json");
    let mut it = env::args().skip(1);
    while let Some(arg) = it.next() {
        match arg.as_str() {
            "--base" => base = Some(PathBuf::from(it.next().ok_or("--base needs a path")?)),
            "--out" => out = PathBuf::from(it.next().ok_or("--out needs a path")?),
            "--help" | "-h" => {
                println!("Usage: semantic_graph_solver --base /path/to/tt-challenge --out submission.json");
                return Ok(());
            },
            _ => return Err(format!("Unknown argument: {arg}").into()),
        }
    }
    let base = base.or_else(|| env::var("TRAINTICKET_BASE").ok().map(PathBuf::from))
        .unwrap_or_else(||PathBuf::from("tt-challenge"));
    let skeleton = read_json(&base.join("skeleton/trainticket.json.gz"))?;
    let snapshot = DbSnapshot::load(&base.join("context/db-after-warmup.sql.gz"))?;
    println!("SQL snapshot: high-speed orders={}, regular orders={}", snapshot.high.len(), snapshot.regular.len());
    let mut index = Index::new(&skeleton)?;
    let recovered = resolve::recover(&mut index, &skeleton, &base.join("targets/trainticket"))?;
    println!("Recovered {recovered} statically typed project calls");
    let mut roots = HashMap::new();
    for &(key,prefix) in ROOT_PREFIXES {
        roots.insert(key, index.method(prefix,true)?.ok_or(format!("Missing {key}"))?);
    }
    let mut output = serde_json::Map::new();
    for i in 0..=7 {
        let sid = format!("S{i}");
        let scenario = read_json(&base.join(format!("scenarios/{sid}.json")))?;
        let steps = parse_scenario(&scenario, &snapshot)?;
        let val = build_block(&index,&roots,&sid,&steps)?;
        let n = val["edges"].as_array().map(Vec::len).unwrap_or(0);
        println!("{sid}: {n} edges");
        output.insert(sid,val);
    }
    let result = json!({"project":str_prop(&skeleton,"project"),"scenarios":Value::Object(output)});
    fs::write(&out, serde_json::to_vec(&result)?)?;
    println!("Wrote {}",out.display());
    Ok(())
}
