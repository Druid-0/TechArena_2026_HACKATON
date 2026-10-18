#!/usr/bin/env python3
"""Offline scenario/path assertions. Reads supplied Java, SQL and scenario JSON.
Does not start any Train Ticket service or derive hidden ground truth.
"""
import argparse, gzip, json, pathlib, re
p=argparse.ArgumentParser()
p.add_argument('--base',required=True,type=pathlib.Path)
a=p.parse_args()
b=a.base
src=lambda module,path: (b/'targets/trainticket'/module/'src/main/java'/path).read_text()
sc={i:json.loads((b/f'scenarios/S{i}.json').read_text()) for i in (2,3)}
steps={i:[row['action'].lower() for row in sc[i]['steps']] for i in (2,3)}
assert len(steps[2])==14 and len(steps[3])==15
assert sum(a.startswith('book ') for a in steps[2])==2
assert sum(a.startswith('book ') for a in steps[3])==2
assert sum('pay for' in a for a in steps[2])==2
assert sum('pay for' in a for a in steps[3])==2
assert 'cheaper' in steps[2][-1]
assert 'without food, assurance or consignment' in steps[3][7]
refund=src('ts-cancel-service','cancel/service/CancelServiceImpl.java')
rebook=src('ts-rebook-service','rebook/service/RebookServiceImpl.java')
payment=src('ts-inside-payment-service','inside_payment/service/InsidePaymentServiceImpl.java')
ph=src('ts-preserve-service','preserve/service/PreserveServiceImpl.java')
pr=src('ts-preserve-other-service','preserveOther/service/PreserveOtherServiceImpl.java')
# Branch guards and order follow-through as implemented in Java.
assert refund.index('getOrderByIdFromOrder(orderId, headers)') < refund.index('getOrderByIdFromOrderOther(orderId, headers)')
assert refund.index('cancelFromOtherOrder(order, headers)') < refund.index('drawbackMoney(money, loginId, headers)',refund.index('cancelFromOtherOrder(order, headers)'))
assert rebook.index('if (priceOld.compareTo(priceNew) > 0)') < rebook.index('drawBackMoney(info.getLoginId(), difference, httpHeaders)') < rebook.index('return updateOrder(order, info, (TripAllDetail) gtdr.getData(), ticketPrice, httpHeaders)')
assert rebook.index('if ((tripGD(oldTripId) && tripGD(info.getTripId()))') < rebook.index('updateOrder(order, info.getTripId(), httpHeaders)') < rebook.index('deleteOrder(order.getId().toString(), oldTripId, httpHeaders)')
assert payment.index('if (totalExpand.compareTo(money) > 0)') < payment.index('} else {\n                setOrderStatus(')
for preserve in (ph,pr):
 assert preserve.index('if (oti.getAssurance() == 0)') < preserve.index('addAssuranceForOrder(' , preserve.index('if (oti.getAssurance() == 0)'))
 assert preserve.index('if (oti.getFoodType() != 0)') < preserve.index('createFoodOrder(foodOrder,')
 assert preserve.index('if (null != oti.getConsigneeName()') < preserve.index('createConsign(consignRequest,')
with gzip.open(b/'context/db-after-warmup.sql.gz','rt',errors='replace') as f: sql=f.read()
assert 'INSERT INTO `orders` VALUES ' in sql
assert 'INSERT INTO `orders_other` VALUES ' in sql
# Independent miniature state transition oracle, not used for predictions.
class Orders:
 def __init__(self):self.states=[];self.refunds=0;self.diff_refunds=0
 def book(self):self.states.append('RESERVED')
 def pay(self):
  i=next(i for i,s in enumerate(self.states) if s=='RESERVED')
  self.states[i]='PAID'
 def cancel(self):
  i=max(i for i,s in enumerate(self.states) if s in ('PAID','RESERVED'))
  self.refunds+=self.states[i]=='PAID';self.states[i]='CANCELLED'
 def rebook_cheaper(self):
  i=max(i for i,s in enumerate(self.states) if s=='PAID')
  self.diff_refunds+=1;self.states[i]='CHANGED'
state2=Orders()
state2.book();state2.pay();state2.cancel();state2.book();state2.pay();state2.rebook_cheaper()
assert state2.states==['CANCELLED','CHANGED'] and (state2.refunds,state2.diff_refunds)==(1,1)
state3=Orders();state3.book();state3.book();state3.pay();assert state3.states==['PAID','RESERVED'];state3.pay();assert state3.states==['PAID','PAID']
print('PASS S2 branch audit: cancellation, refund and cheaper same-family rebooking')
print('PASS S3 branch audit: 2 separate reservations, 2 internal payments and optional services')
print('PASS SQL baseline present; no services were started')
print('TRACE S2:',state2.states, 'refunds=',state2.refunds,'difference_refunds=',state2.diff_refunds)
print('TRACE S3:',state3.states)
