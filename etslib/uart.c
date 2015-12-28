#include <esp8266.h>

#define UART_TASK_ID 0

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;

static os_event_t uart_evt_queue[16];

static void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
 ******************************************************************************/
static void ICACHE_FLASH_ATTR uart_config(uint8 uart_no)
{
	if (uart_no == UART1) {
		PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
	}
	else {
		ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, NULL);
		PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
		PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
#ifdef UART_HW_RTS
		PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS); //HW FLOW CONTROL RTS PIN
#endif
#ifdef UART_HW_CTS
		PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_U0CTS); //HW FLOW CONTROL CTS PIN
#endif
	}

	uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate));

	WRITE_PERI_REG(UART_CONF0(uart_no),
			((UartDev.exist_parity & UART_PARITY_EN_M) << UART_PARITY_EN_S) //SET BIT AND PARITY MODE
			| ((UartDev.parity & UART_PARITY_M) <<UART_PARITY_S ) | ((UartDev.stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S) | ((UartDev.data_bits & UART_BIT_NUM) << UART_BIT_NUM_S));

	// clear rx and tx fifo,not ready
	SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);//RESET FIFO
	CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

	if (uart_no == UART0) {
		//set rx fifo trigger
		WRITE_PERI_REG(UART_CONF1(uart_no),
				((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
#ifdef UART_HW_RTS
				((110 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) |
				UART_RX_FLOW_EN |   //enbale rx flow control
#endif
				(0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S | UART_RX_TOUT_EN| ((0x10 & UART_TXFIFO_EMPTY_THRHD)<<UART_TXFIFO_EMPTY_THRHD_S)); //wjl
#ifdef UART_HW_CTS
				SET_PERI_REG_MASK( UART_CONF0(uart_no),UART_TX_FLOW_EN); //add this sentense to add a tx flow control via MTCK( CTS )
#endif
		SET_PERI_REG_MASK(UART_INT_ENA(uart_no),
				UART_RXFIFO_TOUT_INT_ENA |UART_FRM_ERR_INT_ENA);
	}
	else {
		WRITE_PERI_REG(UART_CONF1(uart_no),
				((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S)); //TrigLvl default val == 1
	}

	// clear all interrupt
	WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
	// enable rx_interrupt
	SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA);
}

/******************************************************************************
 * FunctionName : uart_tx_one_char
 * Description  : Internal used function
 *                Use uart interface to transfer one char
 * Parameters   : uint8 uart - interface to use
 *                uint8 TxChar - character to tx
 * Returns      : OK
 ******************************************************************************/
STATUS uart_tx_one_char(uint8 uart, uint8 TxChar)
{
	while (true) {
		uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(uart))
				& (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
		if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
			break;
		}
	}
	WRITE_PERI_REG(UART_FIFO(uart), TxChar);
	return OK;
}

/******************************************************************************
 * FunctionName : uart0_putchar
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
 ******************************************************************************/
void ICACHE_FLASH_ATTR uart0_putchar(char c)
{
	if (c == '\n') {
		uart_tx_one_char(UART0, '\r');
		uart_tx_one_char(UART0, '\n');
	}
	else if (c == '\r') {

	}
	else {
		uart_tx_one_char(UART0, c);
	}
}

/******************************************************************************
 * FunctionName : uart1_putchar
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
 ******************************************************************************/
void ICACHE_FLASH_ATTR uart1_putchar(char c)
{
	if (c == '\n') {
		uart_tx_one_char(UART1, '\r');
		uart_tx_one_char(UART1, '\n');
	}
	else if (c == '\r') {

	}
	else {
		uart_tx_one_char(UART1, c);
	}
}

/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
 ******************************************************************************/

static void uart0_rx_intr_handler(void *para)
{
	/* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
	 * uart1 and uart0 respectively
	 */

	uint8 uart_no = UART0;

	if (UART_FRM_ERR_INT_ST
			== (READ_PERI_REG(UART_INT_ST(UART0)) & UART_FRM_ERR_INT_ST)) {
		// frame error
		WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
	}

	if (UART_RXFIFO_FULL_INT_ST
			== (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST)) {
		// fifo full
		goto read_chars;
	}
	else if (UART_RXFIFO_TOUT_INT_ST
			== (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_TOUT_INT_ST)) {
		read_chars:
		ETS_UART_INTR_DISABLE();
		system_os_post(UART_TASK_ID, 0, 0);
	}
}

int uart_rx_one_char(uint8 uart_no)
{
	if (READ_PERI_REG(UART_STATUS(uart_no))
			& (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
		return READ_PERI_REG(UART_FIFO(uart_no)) & 0xff;
	}
	return -1;
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 * Returns      : NONE
 ******************************************************************************/
void ICACHE_FLASH_ATTR uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{
	// rom use 74880 baut_rate, here reinitialize
	UartDev.baut_rate = uart0_br;
	uart_config(UART0);
	UartDev.baut_rate = uart1_br;
	uart_config(UART1);
	ETS_UART_INTR_ENABLE();

	// install uart1 putc callback
	os_install_putc1((void *)uart1_putchar);
}

// Task-based UART interface

extern void repl_process_char(char c);

void ICACHE_FLASH_ATTR uart_task_handler(os_event_t *evt)
{
	int c;

	while ((c = uart_rx_one_char(UART0)) >= 0) {
		repl_process_char(c);
	}

	// Clear pending FIFO interrupts
	WRITE_PERI_REG(UART_INT_CLR(UART0),
			UART_RXFIFO_TOUT_INT_CLR | UART_RXFIFO_FULL_INT_ST);
	// Enable UART interrupts, so our task will receive events again from IRQ handler
	ETS_UART_INTR_ENABLE();
}

void ICACHE_FLASH_ATTR uart_task_init(void)
{
	system_os_task(uart_task_handler, UART_TASK_ID, uart_evt_queue,
			sizeof(uart_evt_queue) / sizeof(*uart_evt_queue));
}

int ICACHE_FLASH_ATTR uart0_vprintf(const char *format, va_list ap)
{
	char buf[128], *p;
	int str_l;

	str_l = ets_vsnprintf(buf, sizeof(buf), format, ap);
	p = buf;
	while (*p) {
		uart0_putchar(*p);
		p++;
	}
	return str_l;
}

int ICACHE_FLASH_ATTR uart0_printf(const char *format, ...)
{
	va_list ap;
	int str_l;

	va_start(ap, format);
	str_l = uart0_vprintf(format, ap);
	va_end(ap);
	return str_l;
}
