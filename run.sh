#!/bin/bash

# Change localtime to Taipei
# cp /usr/share/zoneinfo/Asia/Taipei /etc/localtime

ORACLE="0xOracleAddress"
SUPPLIER="0x1234"
BUYER="0x4567"
LENDER="0x5555"
ORDERPRICE=2000
ORIGINAL_D=20200320
EARLY_D=20200120
METADATA="contract-orderList"

IS_RECEIVED=1
S_ERP_CREDIT=82
B_ERP_CREDIT=85
MIN_CREDIT=80

ourcontract-mkdll contracts sfc2
ourcontract-rt contracts sfc2 init

# Token info
ourcontract-rt contracts sfc2 symbol
ourcontract-rt contracts sfc2 name
ourcontract-rt contracts sfc2 decimal
ourcontract-rt contracts sfc2 totalSupply

# Sign-up users and lender
ourcontract-rt contracts sfc2 user_sign_up $SUPPLIER $S_ERP_CREDIT
ourcontract-rt contracts sfc2 user_sign_up $BUYER $B_ERP_CREDIT
ourcontract-rt contracts sfc2 lender_sign_up $LENDER $MIN_CREDIT
ourcontract-rt contracts sfc2 printUsers

# Buy Token
ourcontract-rt contracts sfc2 buyToken $BUYER 2000
ourcontract-rt contracts sfc2 buyToken $LENDER 10000

# Check their balances
ourcontract-rt contracts sfc2 balanceOf $ORACLE
ourcontract-rt contracts sfc2 balanceOf $SUPPLIER
ourcontract-rt contracts sfc2 balanceOf $BUYER
ourcontract-rt contracts sfc2 balanceOf $LENDER

# Start lending...
ourcontract-rt contracts sfc2 createLoan $SUPPLIER $BUYER $ORDERPRICE 80 10 $ORIGINAL_D $EARLY_D $METADATA
ourcontract-rt contracts sfc2 printLoan 0 # contract that can be seen by everyone
ourcontract-rt contracts sfc2 buyerPrintLoan 0 $BUYER # shows metadata: contract and order list
ourcontract-rt contracts sfc2 buyerApproveLoan 0 $BUYER
ourcontract-rt contracts sfc2 printLoan 0
ourcontract-rt contracts sfc2 lenderApproveProvideLoan 0 $LENDER 1574
ourcontract-rt contracts sfc2 allowance $LENDER $ORACLE
ourcontract-rt contracts sfc2 balanceOf $SUPPLIER
ourcontract-rt contracts sfc2 borrowerWithdrawLoan 0 $SUPPLIER
ourcontract-rt contracts sfc2 allowance $LENDER $ORACLE
ourcontract-rt contracts sfc2 balanceOf $SUPPLIER
ourcontract-rt contracts sfc2 buyerCheckProductReceived 0 $BUYER $IS_RECEIVED
ourcontract-rt contracts sfc2 printLoan 0
ourcontract-rt contracts sfc2 buyerPayLoan 0 $BUYER $ORDERPRICE
ourcontract-rt contracts sfc2 printLoan 0
ourcontract-rt contracts sfc2 oracleUpdateBuyerCredit 0

# Check their balances
ourcontract-rt contracts sfc2 balanceOf $SUPPLIER
ourcontract-rt contracts sfc2 allowance $BUYER $ORACLE
ourcontract-rt contracts sfc2 balanceOf $BUYER
ourcontract-rt contracts sfc2 balanceOf $LENDER
ourcontract-rt contracts sfc2 allowance $BUYER $ORACLE

# Check their credits
ourcontract-rt contracts sfc2 printUsers
