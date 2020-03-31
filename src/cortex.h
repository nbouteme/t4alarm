#ifndef CORTEX_H
#define CORTEX_H

struct SystickCSR {
	uint8_t enable : 1;
	uint8_t tickint : 1;
	uint8_t clk_source : 1;
	uint16_t : 13;
	uint8_t reached_zero : 1;
};

struct ICSR {
	uint16_t vectactive : 9;
	uint8_t : 2;
	uint8_t rettobase : 1;
	uint16_t vectpending : 9;
	uint8_t : 1;
	uint8_t isr_pending: 1;
	uint8_t _ro: 1;
	uint8_t : 1;
	uint8_t pendstclr : 1;
	uint8_t pendstset : 1;
	uint8_t pendsvclr : 1;
	uint8_t pendsvset : 1;
	uint8_t : 2;
	uint8_t nmi_pend_set : 1;
};

#endif /* CORTEX_H */