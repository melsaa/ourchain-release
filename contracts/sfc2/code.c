#define __USE_XOPEN // strptime
#define _GNU_SOURCE // strptime

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ourcontract.h>
#include "orc20.h"

#define MAX_USER 1000
#define MAX_LENDERS 1000
#define MAX_LOANS 1000
// Oracle is an agent that supplies a convertion rate between ORC20 and NTD
#define ORACLE_ADDR "0xOracleAddress"

// safe_math functions
uint64_t safeAdd(uint64_t x, uint64_t y);
uint64_t safeSubtract(uint64_t x, uint64_t y);
uint64_t safeMult(uint64_t x, uint64_t y);
uint64_t min(uint64_t a, uint64_t b);
uint64_t max(uint64_t a, uint64_t b);

typedef struct { 
    char x[50];
} address;

address address_new(char* a) {
    address* p = malloc(sizeof(address));
    strcpy(p->x, a);
    return *p;
}

typedef enum Status { initial, lent, paid, destroyed } Status;

// Address Description : address and amount
typedef struct {
    address _address;
    uint64_t _amount;
} AddressDesc;

// We assume that buyer won't delay payments
// And borrower won't deliver product late
typedef struct {
    Status status;
    //Oracle oracle; // an agent that supplies a convertion rate between ORC20 and NTD
    address borrower; // supplier
    address buyer;
    address lender;
    // address cosigner; // insurance company
    uint64_t borrowerCredit;
    uint64_t buyerCredit;

    uint64_t amount;
    uint64_t lendPercentage; // percentage of production cost
    uint64_t lentAmount;
    uint64_t interest;
    double interestRate;
    char paymentDate[10]; // YYYYMMDD
    char earlyPaymentDate[10]; // YYYYMMDD
    
    char productReceived; // 1 or -1
    char paid; // 1 or -1

    address approvedTransfer;
    char approvedByBuyer; // 1 or -1

    char metadata[100]; // hashed contract & order list
} Loan;

typedef struct {
    int activeLoans;
    int lendersLength;
    AddressDesc lendersBalance[MAX_LENDERS];
    Loan loans[MAX_LOANS];
} State;

State state;

static int user_sign_up(char* address) {
    appendToAccountArray(createAccount(address));
    return 0;
}

static int lender_sign_up(char* address) {
    int success = user_sign_up(address);
    strcpy(state.lendersBalance[state.lendersLength]._address.x, address);
    state.lendersBalance[state.lendersLength]._amount = 0;
    state.lendersLength += 1;
    return success;
}

static void print_all_lenders() {
    err_printf("%d\n", state.lendersLength);
    for (int i = 0; i < state.lendersLength; i++) {
        err_printf("%d %s\n", i, state.lendersBalance[i]._address.x);
    }
}

static int user_buyToken(char *address, int amount) {
    int trans = transfer(ORACLE_ADDR, address, amount);
    if (trans != -1) {
        return approve(ORACLE_ADDR, address, amount);
    }
    return trans;
}

static int lender_buyToken(char *address, int amount) {
    for (int i = 0; i < state.lendersLength; i++) {
        if (strcmp(state.lendersBalance[i]._address.x, address) == 0) {
            state.lendersBalance[i]._amount = amount;
            return user_buyToken(address, amount);
        }
    }

    return -1;
}

/*
    The following functions are used to 
        * store your program data
        * read your program data
        * data structure related method
        * serialize data structure
*/

static unsigned int readSCFState(unsigned char* buffer, unsigned int offset)
{
    memcpy(&state, buffer+offset, sizeof(State));
    return sizeof(State);
}

static unsigned int readState()
{
    /*
        Use state_read() to read your program data
        The data are stored in memory, tight together with UTXO so it will revert automatically

        state_read(buff, size) is straightforward: read `size` bytes to `buff`
        The point is how you define your structure and serialize it

        The following code is just one of the way to read state
            * In write stage: 
            * you first write how many byte you stored
            * then write all your data
            * In read stage:
            * first get the size of data
            * then get all the data
            * unserialize the data    
    */

    unsigned int count;
    state_read(&count, sizeof(int));

    unsigned char* buff = malloc(sizeof(char) * count);
    unsigned int offset = 0;
    state_read(buff, count);
    
    offset += readContractState(buff, offset);
    offset += readToken(buff, offset);
    offset += readAccountArray(buff, offset);
    offset += readAllowanceArray(buff, offset);
    offset += readSCFState(buff, offset);

    if (offset != count) {
        err_printf("offset = %u  count = %u\n", offset, count);
        assert(offset == count);
    }
    return offset;
}

static unsigned int compute_SCFContract_size() {
    unsigned int sum = 0;

    unsigned int size_loan = sizeof(Loan) * MAX_LOANS;
    unsigned int size_address_desc = sizeof(AddressDesc) * MAX_LENDERS;

    sum = sizeof(int) * 2 + size_loan + size_address_desc;

    return sum;
}

static unsigned int writeSCFStateToState(unsigned char* buffer, unsigned int offset)
{
    memcpy(buffer+offset, &state, sizeof(State));
    return sizeof(State);
}

static unsigned int writeState()
{
    /*
        Use state_write() to write your program data
        The data are stored in memory, tight together with UTXO so it will revert automatically

        state_read(buff, size) is straightforward: write `size` bytes from `buff`
        
        Warning: You need to write all your data at once. 
        The state is implement as a vector, and will resize every time you use state_write
        So if you write multiple times, it will be the size of last write

        One way to solve this is you memcpy() all your serialized data to a big array
        and then call only one time state_write()
    */

    unsigned int total_size = theContractState.size_contract + compute_SCFContract_size();
    unsigned char *buff = malloc(sizeof(int) + sizeof(char) * total_size);
    unsigned int offset = 0;

    memcpy(buff, &total_size, sizeof(int));
    offset += sizeof(int);

    offset += writeContractStateToState(buff, offset);
    offset += writeTokenToState(buff, offset);
    offset += writeAccountArrayToState(buff, offset);
    offset += writeAllowanceArrayToState(buff, offset);
    offset += writeSCFStateToState(buff, offset);

    assert(offset == sizeof(int) + sizeof(char)* total_size);
    state_write(buff, offset);
    return offset;
}

/*
 * Source: https://www.scfbriefing.com/the-key-to-unlocking-the-potential-of-supply-chain-finance/
 * days = paymentDate - earlyPaymentDate
 * interest = interestRate * (amount * lendPercentage) * days / 360
 */
uint64_t calculateInterest(Loan loan) {
    double seconds;
    uint64_t interest;
    uint days;
    struct tm ltm1 = {0};
    struct tm ltm2 = {0};

    strptime(loan.paymentDate, "%Y%m%d", &ltm1);
    strptime(loan.earlyPaymentDate, "%Y%m%d", &ltm2);
    seconds = difftime(mktime(&ltm1), mktime(&ltm2));
    days = seconds / 86400;
    
    interest = max(loan.interestRate * loan.amount * loan.lendPercentage * days / 3600000, 1);

    return interest;
}

uint64_t calculateLentAmount(Loan loan) {
    uint64_t lentAmount;

    lentAmount = (loan.amount * loan.lendPercentage / 100) - loan.interest;

    return lentAmount;
}

uint64_t borrower_create_loan(char* _borrower,
                              char* _buyer,
                              uint64_t _amount,
                              uint64_t _percentage,
                              uint64_t _interestRate,
                              char* _paymentDate, 
                              char* _earlyPaymentDate,
                              char* _metadata) {
    Loan loan;
    loan.status = initial;
    loan.borrower = address_new(_borrower);
    loan.buyer = address_new(_buyer);
    loan.lender = address_new("0x0");
    loan.amount = _amount;
    loan.lendPercentage = _percentage;
    loan.interestRate = _interestRate;
    strcpy(loan.paymentDate, _paymentDate);
    strcpy(loan.earlyPaymentDate, _earlyPaymentDate);
    loan.interest = calculateInterest(loan);
    loan.lentAmount = calculateLentAmount(loan);
    loan.approvedByBuyer = -1;
    loan.productReceived = -1;
    loan.paid = -1;
    strcpy(loan.metadata, _metadata);

    uint64_t index = state.activeLoans;
    state.loans[state.activeLoans++] = loan;

    return index;
}

uint64_t public_print_loan(uint64_t index) {
    if (index < state.activeLoans) {
        Loan loan = state.loans[index];
        err_printf("\nLoan #%d\n", index);
        err_printf("====================================\n");
        err_printf("Status               : ");
        if (loan.status == initial) err_printf("initial\n");
        else if (loan.status == lent) err_printf("lent\n");
        else if (loan.status == paid) err_printf("paid\n");
        else if (loan.status == destroyed) err_printf("destroyed\n");
        err_printf("Borrower             : %s\n", loan.borrower.x);
        err_printf("Buyer                : %s\n", loan.buyer.x);
        err_printf("Lender               : %s\n", loan.lender.x);
        err_printf("Amount               : %d\n", loan.amount);
        err_printf("LendPercentage       : %d\n", loan.lendPercentage);
        err_printf("LentAmount           : %d\n", loan.lentAmount);
        err_printf("InterestRate         : %f\n", loan.interestRate);
        err_printf("Interest             : %d\n", loan.interest);
        err_printf("PaymentDate          : %s\n", loan.paymentDate);
        err_printf("EarlyPaymentDate     : %s\n", loan.earlyPaymentDate);
        err_printf("ApprovedByBuyer      : %d\n", loan.approvedByBuyer);
        err_printf("IsProductReceived    : %d\n", loan.productReceived);
        err_printf("IsLoanPaid           : %d\n", loan.paid);
        return 1;
    }
    return -1;
}

uint64_t buyer_print_loan(uint64_t index, char* _buyer) {
    Loan loan = state.loans[index];
    if (strcmp(loan.buyer.x, _buyer) == 0) {
        if(public_print_loan(index)) {
            err_printf("Metadata             : %s\n", loan.metadata);
            return 1;
        }
    }
    return -1;
}

uint64_t buyer_approveLoan(uint64_t index, char* _buyer) {
    Loan *loan = &state.loans[index];
    if (loan->status == initial && !strcmp(loan->buyer.x, _buyer)) {
        loan->approvedByBuyer = 1;
        return 1;
    }
    return -1;
}

uint64_t lender_approve_provideLoan(uint64_t index, char* _lender, uint64_t _lentAmount) {
    Loan *loan = &state.loans[index];
    if (loan->status == initial && !strcmp(loan->lender.x, "0x0") && _lentAmount == loan->lentAmount) {
        if (loan->approvedByBuyer) {
            strcpy(loan->lender.x, _lender);
            loan->status = lent;
            if (transfer(loan->lender.x, loan->borrower.x, loan->lentAmount) == 0) {
                return 1;
            } else {
                err_printf("Transfer failed\n");
            }
        } else {
            err_printf("Contract hasn't been approved by buyer\n");
        }
    } else {
        err_printf("lentAmount should be %d\n", loan->lentAmount);
    }
    return -1;
}

uint64_t borrower_withdrawLoan(uint64_t index, char* _borrower) {
    Loan *loan = &state.loans[index];
    if (loan->status == lent && !strcmp(loan->borrower.x, _borrower)) {
        if (approve(loan->lender.x, loan->borrower.x, loan->lentAmount) == 0) {
            return 1;
        }
    }
    return -1;
}

uint64_t buyer_checkProductReceived(uint64_t index, char *_buyer) {
    Loan *loan = &state.loans[index];
    if (loan->status == lent && !strcmp(loan->buyer.x, _buyer) && loan->productReceived == -1) {
        loan->productReceived = 1;
        return 1;
    }
    return -1;
}

uint64_t buyer_payLoan(uint64_t index, char* _buyer, uint64_t _amount) {
    Loan *loan = &state.loans[index];
    if (loan->status == lent && !strcmp(loan->buyer.x, _buyer) && _amount == loan->amount
        && loan->productReceived == 1) {
        loan->status = paid;
        loan->paid = 1;
        if (transfer(loan->buyer.x, loan->lender.x, _amount) == 0) {
            return 1;
        }
    }
    return -1;
}

static void state_init()
{
    state.activeLoans = 0;
    state.lendersLength = 0;
}

static int too_few_args()
{
    err_printf("too few args\n");
    return -1;
}

int contract_main(int argc, char** argv)
{
    if (argc < 2) {
        too_few_args();
        return -1;
    }

    if (!strcmp(argv[1], CONTRACT_INIT_FUNC)) {
        err_printf("init contract\n");

        // contract-related data
        strcpy(ourToken.contractOwnerAddress, ORACLE_ADDR);
        strcpy(ourToken.name, "OurToken");
        strcpy(ourToken.symbol, "OTK");
        ourToken.decimal = 0;
        ourToken.totalSupply = 1e8;

        // contract-state data
        initAccountArray();
        initAllowanceArray();
        theContractState.size_contract = compute_contract_size();
        state_init();

        writeState();
    } else {
        readState();

        if (!strcmp(argv[1], "symbol")) {
            err_printf("symbol:%s\n", symbol());
        } else if (!strcmp(argv[1], "name")) {
            err_printf("name:%s\n", name());
        } else if (!strcmp(argv[1], "decimal")) {
            err_printf("decimals:%d\n", decimals());
        } else if (!strcmp(argv[1], "totalSupply")) {
            print_all_lenders();
            err_printf("totalSuply:%d\n", totalSupply());
        } else if (!strcmp(argv[1], "user_sign_up")) {
            if (argc != 3) {
                err_printf("%s: usage: sfc2 user_sign_up user_address\n", argv[0]);
                return -1;
            }
            int ret = user_sign_up(argv[2]);
            if (ret != 0) {
                err_printf("%s: user_sign_up failed\n", argv[0]);
                return -1;
            }
            err_printf("userSignUp:%d\n", ret);
        } else if (!strcmp(argv[1], "lender_sign_up")) {
            if (argc != 3) {
                err_printf("%s: usage: sfc2 lender_sign_up user_address\n", argv[0]);
                return -1;
            }
            int ret = lender_sign_up(argv[2]);
            if (ret != 0) {
                err_printf("%s: lender_sign_up failed\n", argv[0]);
                return -1;
            }
            err_printf("lenderSignUp:%d\n", ret);
        } else if (!strcmp(argv[1], "balanceOf")) {
            if (argc < 3) {
                err_printf("%s: usage: scf2 balanceOf user_address\n", argv[0]);
                return -1;
            }
            err_printf("balanceOf %s:%d\n", argv[2], balanceOf(argv[2]));
        } else if (!strcmp(argv[1], "createLoan")) {
            if (argc != 10) {
                err_printf("%s: usage: scf2 createLoan borrower_address buyer_address amount lendPercentage interestRate paymentDate (YYYYMMDD) earlyPaymentDate(YYYYMMDD) metadata\n", argv[0]);
                return -1;
            }
            err_printf("createLoan:%d\n", borrower_create_loan(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), argv[7], argv[8], argv[9]));
        } else if (!strcmp(argv[1], "printLoan")) {
            if (argc != 3) {
                err_printf("%s: usage: scf2 printLoan loan_index\n", argv[0]);
                return -1;
            }
            public_print_loan(atoi(argv[2]));
        } else if (!strcmp(argv[1], "buyerPrintLoan")) {
            if (argc != 4) {
                err_printf("%s: usage: scf2 buyerPrintLoan loan_index buyer_address\n", argv[0]);
                return -1;
            }
            err_printf("buyerPrintLoan:%d\n", buyer_print_loan(atoi(argv[2]), argv[3]));
        } else if (!strcmp(argv[1], "buyerApproveLoan")) {
            if (argc != 4) {
                err_printf("%s: usage: scf2 buyerApproveLoan loan_index buyer_address\n", argv[0]);
                return -1;
            }
            err_printf("buyerApproveLoan:%d\n", buyer_approveLoan(atoi(argv[2]), argv[3]));
        } else if (!strcmp(argv[1], "lenderApproveProvideLoan")) {
            if (argc != 5) {
                err_printf("%s: usage: scf2 lenderApproveProvideLoan loan_index lender_address lent_amount\n", argv[0]);
                return -1;
            }
            err_printf("lenderApproveProvideLoan:%d\n", lender_approve_provideLoan(atoi(argv[2]), argv[3], atoi(argv[4])));
        } else if (!strcmp(argv[1], "borrowerWithdrawLoan")) {
            if (argc != 4) {
                err_printf("%s: usage: scf2 borrowerWithdrawLoan loan_index borrower_address\n", argv[0]);
                return -1;
            }
            err_printf("borrowerWithdrawLoan:%d\n", borrower_withdrawLoan(atoi(argv[2]), argv[3]));
        } else if (!strcmp(argv[1], "buyerPayLoan")) {
            if (argc != 5) {
                err_printf("%s: usage: scf2 buyerPayLoan loan_index buyer_address paid_amount\n", argv[0]);
                return -1;
            }
            err_printf("buyerPayLoan:%d\n", buyer_payLoan(atoi(argv[2]), argv[3], atoi(argv[4])));
        } else if (!strcmp(argv[1], "buyerCheckProductReceived")) {
            if (argc != 4) {
                err_printf("%s: usage: scf2 buyerCheckProductReceived loan_index buyer_address\n");
                return -1;
            }
            err_printf("buyerCheckProductReceived:%d\n", buyer_checkProductReceived(atoi(argv[2]), argv[3]));
        } else if (!strcmp(argv[1], "allowance")) {
            if (argc < 4) {
                err_printf("%s: usage: scf2 allowance token_owner_address spender_address\n", argv[0]);
                return -1;
            }
            err_printf("allowance:%d\n", allowance(argv[2], argv[3]));
        } else if (!strcmp(argv[1], "user_buyToken")) {
            if (argc != 4) {
                err_printf("%s: usage: scf2 user_buyToken user_address token_amount\n", argv[0]);
                return -1;
            }
            err_printf("user_buyToken:%d\n", user_buyToken(argv[2], atoi(argv[3])));
        } else if (!strcmp(argv[1], "lender_buyToken")) {
            if (argc != 4) {
                err_printf("%s: usage: scf2 lender_buyToken lender_address token_amount\n", argv[0]);
                return -1;
            }
            int success = lender_buyToken(argv[2], atoi(argv[3]));
            if (success == -1) {
                err_printf("Are you a lender?\n");
            }
            err_printf("lender_buyToken:%d\n", success);
        } else if (!strcmp(argv[1], "sellToken")) {
            return 0;
        } else {
            err_printf("error:command not found:%s\n", argv[1]);
            return 0;
        }

        theContractState.size_contract = compute_contract_size();
        writeState();
    }

    return 0;
}

uint64_t safeAdd(uint64_t x, uint64_t y) {
    uint64_t z = x + y;
    assert((z >= x) && (z >= y));
    return z;
}

uint64_t safeSubtract(uint64_t x, uint64_t y) {
    assert(x >= y);
    uint64_t z = x - y;
    return z;
}

uint64_t safeMult(uint64_t x, uint64_t y) {
    uint64_t z = x * y;
    assert((x == 0) || (z/x == y));
    return z;
}

uint64_t min(uint64_t a, uint64_t b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

uint64_t max(uint64_t a, uint64_t b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}