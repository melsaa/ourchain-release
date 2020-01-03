#!/bin/bash

ourcontract-mkdll contracts sfc2
ourcontract-rt contracts sfc2 init

# Sign-up users and lender
ourcontract-rt contracts sfc2 user_sign_up 0x1234 # Supplier
ourcontract-rt contracts sfc2 user_sign_up 0x4567 # Buyer
ourcontract-rt contracts sfc2 lender_sign_up 0x5555 # Lender

# Buy Token
ourcontract-rt contracts sfc2 user_buyToken 0x4567 200
ourcontract-rt contracts sfc2 lender_buyToken 0x5555 1000

# Check their balances
ourcontract-rt contracts sfc2 balanceOf 0xOracleAddress
ourcontract-rt contracts sfc2 balanceOf 0x1234
ourcontract-rt contracts sfc2 balanceOf 0x4567
ourcontract-rt contracts sfc2 balanceOf 0x5555

# Start lending...
ourcontract-rt contracts sfc2 createLoan 0x1234 0x4567 200 80 1 20200320 20200120 contract-orderList
ourcontract-rt contracts sfc2 printLoan 0 # contract that can be seen by everyone
ourcontract-rt contracts sfc2 buyerPrintLoan 0 0x4567 # shows metadata: contract and order list
ourcontract-rt contracts sfc2 buyerApproveLoan 0 0x4567
ourcontract-rt contracts sfc2 printLoan 0
ourcontract-rt contracts sfc2 lenderApproveProvideLoan 0 0x5555 159
ourcontract-rt contracts sfc2 printLoan 0
ourcontract-rt contracts sfc2 borrowerWithdrawLoan 0 0x1234
ourcontract-rt contracts sfc2 buyerCheckProductReceived 0 0x4567
ourcontract-rt contracts sfc2 buyerPayLoan 0 0x4567 200
ourcontract-rt contracts sfc2 printLoan 0
