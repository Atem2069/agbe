#include"Input.h"

Input::Input()
{
	keyInput = 0xFFFF;
}

Input::~Input()
{

}

void Input::registerInput(std::shared_ptr<InputState> inputState)
{
	m_inputState = inputState;
	keyInput = 0xFFFF;
	KEYCNT = 0;
}

void Input::registerInterrupts(std::shared_ptr<InterruptManager> interruptManager)
{
	m_interruptManager = interruptManager;
}

void Input::tick(bool skipIrqCheck)
{
	uint16_t newInputState = (~(m_inputState->reg)) & 0x3FF;
	bool shouldCheckIRQ = (newInputState != keyInput);
	keyInput = newInputState;
	if (shouldCheckIRQ)					//i'm confused.. if the irq was already asserted when KEYCNT written, then we can just trigger the irq on key input change
	{									//....otherwise, recheck if the irq can happen?? wtf is my code doing??
		if (irqActive)
			m_interruptManager->requestInterrupt(InterruptType::Keypad);
		else
			checkIRQ(skipIrqCheck);
	}
}

uint8_t Input::readIORegister(uint32_t address)
{
	switch (address)
	{
	case 0x04000130:
		return keyInput & 0xFF;
	case 0x04000131:
		return ((keyInput >> 8) & 0xFF);
	case 0x04000132:
		return KEYCNT & 0xFF;
	case 0x04000133:
		return ((KEYCNT >> 8) & 0xFF);
	}
}

void Input::writeIORegister(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000132:
		KEYCNT &= 0xFF00; KEYCNT |= value;
		break;
	case 0x04000133:
		KEYCNT &= 0xFF; KEYCNT |= (value << 8);
		checkIRQ();
		break;
	}
}

void Input::checkIRQ(bool skipIrqCheck)
{
	irqActive = false;
	bool irqEnabled = ((KEYCNT >> 14) & 0b1);
	if (!irqEnabled && !skipIrqCheck)	//todo: doublecheck. it seems like STOP doesn't care about the irq bit.
		return;
	bool irqMode = ((KEYCNT >> 15) & 0b1);

	bool shouldDoIRQ = (((~keyInput) & 0x3FF) | (KEYCNT & 0x3FF));
	if (irqMode)
		shouldDoIRQ = (((~keyInput) & 0x3FF) == (KEYCNT & 0x3FF));

	if (shouldDoIRQ)
		m_interruptManager->requestInterrupt(InterruptType::Keypad);
	irqActive = shouldDoIRQ;
}
