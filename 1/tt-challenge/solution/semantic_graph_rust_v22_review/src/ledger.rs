//! Scenario-local order state model. No Train Ticket service is launched.
//! SQL is read-only; only scenario-created orders can change in this ledger.
use crate::snapshot::DbSnapshot;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum State { Reserved, Paid, Cancelled, Collected, Changed }

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
struct LocalOrder { family: char, status: State }

pub struct Ledger {
    existing_high: usize,
    existing_regular: usize,
    fresh_account: bool,
    orders: Vec<LocalOrder>,
    refunds: usize,
    difference_refunds: usize,
}
impl Ledger {
    pub fn new(db: &DbSnapshot) -> Self {
        Self { existing_high: db.high.len(), existing_regular: db.regular.len(),
            fresh_account: false, orders: Vec::new(), refunds: 0,
            difference_refunds: 0 }
    }
    pub fn register(&mut self) {
        self.fresh_account = true;
        self.orders.clear();
        self.refunds = 0;
        self.difference_refunds = 0;
    }
    pub fn has_orders(&self, family: char) -> bool {
        if self.orders.iter().any(|o| o.family == family) { return true; }
        if self.fresh_account { return false; }
        match family { 'H' => self.existing_high > 0,
                       'R' => self.existing_regular > 0, _ => false }
    }
    pub fn book(&mut self, family: char) {
        self.orders.push(LocalOrder { family, status: State::Reserved });
    }
    pub fn pay(&mut self) {
        // The user pays the first still-unpaid newly booked order; this is
        // essential in S3, which creates two orders before paying either.
        if let Some(order) = self.orders.iter_mut().find(|o| o.status == State::Reserved) {
            order.status = State::Paid;
        }
    }
    pub fn cancel(&mut self) {
        // S2 cancels its first PAID regular order. COLLECTED cannot be cancelled.
        if let Some(order) = self.orders.iter_mut().rev()
            .find(|o| o.status == State::Paid || o.status == State::Reserved) {
            if order.status == State::Paid { self.refunds += 1; }
            order.status = State::Cancelled;
        }
    }
    pub fn rebook(&mut self) { self.rebook_with_refund(false); }
    pub fn rebook_with_refund(&mut self, cheaper: bool) {
        if let Some(order) = self.orders.iter_mut().rev().find(|o| o.status == State::Paid) {
            order.status = State::Changed;
            if cheaper { self.difference_refunds += 1; }
        }
    }
    pub fn collect(&mut self) {
        if let Some(order) = self.orders.iter_mut().rev()
            .find(|o| o.status == State::Paid || o.status == State::Changed) {
            order.status = State::Collected;
        }
    }
    pub fn is_collected(&self) -> bool {
        self.orders.last().map(|o| o.status == State::Collected).unwrap_or(false)
    }
    #[allow(dead_code)]
    pub fn state_trace(&self) -> String {
        let mut res: Vec<String> = self.orders.iter().enumerate().map(|(i,o)|
            format!("order{}:{}:{:?}",i+1,o.family,o.status)).collect();
        res.push(format!("refunds:{}",self.refunds));
        res.push(format!("difference_refunds:{}",self.difference_refunds));
        res.join(";")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn empty() -> DbSnapshot { DbSnapshot {high:Vec::new(),regular:Vec::new()} }

    #[test]
    fn s2_cancel_and_cheaper_rebooking_keeps_order_identities() {
        let db = empty(); let mut s = Ledger::new(&db);
        s.book('R'); s.pay(); s.cancel();
        assert_eq!(s.orders[0].status,State::Cancelled);
        assert_eq!(s.refunds,1);
        s.book('R'); s.pay(); s.rebook_with_refund(true);
        assert_eq!(s.orders[0].status,State::Cancelled);
        assert_eq!(s.orders[1].status,State::Changed);
        assert_eq!(s.difference_refunds,1);
        assert!(!s.is_collected());
    }
    #[test]
    fn s3_two_orders_are_paid_separately() {
        let db = empty(); let mut s = Ledger::new(&db);
        s.book('H'); s.book('H');
        s.pay();
        assert_eq!(s.orders[0].status,State::Paid);
        assert_eq!(s.orders[1].status,State::Reserved);
        s.pay();
        assert_eq!(s.orders[1].status,State::Paid);
        assert_eq!(s.refunds,0);
    }
    #[test]
    fn s7_collected_order_cannot_be_cancelled_or_rebooked() {
        let db = empty(); let mut s = Ledger::new(&db);
        s.book('H');s.pay();s.collect();s.cancel();s.rebook();
        assert!(s.is_collected());
        assert_eq!(s.refunds,0);
    }
    #[test]
    fn new_account_starts_empty_and_changes_only_after_booking() {
        let mut s=Ledger::new(&empty());s.register();
        assert!(!s.has_orders('H'));
        s.book('H');assert!(s.has_orders('H'));assert!(!s.has_orders('R'));
    }
}
