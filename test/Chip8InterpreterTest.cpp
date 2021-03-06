#include "gtest/gtest.h"

extern "C" {
	#include "chip8/chip8.h"
	#include "chip8/assembler.h"
}

// Helpers

void assert_row(uint8_t x, uint8_t y, uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t v5, uint8_t v6, uint8_t v7){
	ASSERT_EQ(chip8_graphical_memory[x][y], v0);
	ASSERT_EQ(chip8_graphical_memory[x + 1][y], v1);
	ASSERT_EQ(chip8_graphical_memory[x + 2][y], v2);
	ASSERT_EQ(chip8_graphical_memory[x + 3][y], v3);
	ASSERT_EQ(chip8_graphical_memory[x + 4][y], v4);
	ASSERT_EQ(chip8_graphical_memory[x + 5][y], v5);
	ASSERT_EQ(chip8_graphical_memory[x + 6][y], v6);
	ASSERT_EQ(chip8_graphical_memory[x + 7][y], v7);
}

// Tests

TEST(Chip8Interpreter, InitialRegisterState) {
	chip8_init();
	EXPECT_EQ(chip8_get_register_value_unsafe(0), 0);
}

TEST(Chip8Interpreter, AccessInvalidRegister) {
	chip8_init();
	// 255 is an invalid register. Register range [0,15]
	EXPECT_EQ(chip8_get_register_value_safe(255).status, 1);
}

TEST(Chip8Interpreter, AccessInvalidRegisterWithOutputParameter) {
	chip8_init();
	uint8_t value_holder;

	chip8_set_register_value(11, 17);

	chip8_get_register_value_safe2(11, &value_holder);
	EXPECT_EQ(value_holder, 17);
}

TEST(Chip8Interpreter, ModifyRegisters) {
	chip8_init();
	chip8_set_register_value(0, 1);
	EXPECT_EQ(chip8_get_register_value_unsafe(0), 1);
}

TEST(Chip8Interpreter, InitialInstructionPointer) {
	chip8_init();
	EXPECT_EQ(chip8_instruction_pointer, 200);
}

TEST(Chip8Interpreter, StepAdvancesProgramCounter) {
	chip8_init();

	// Clear instruction
	uint8_t bytes[2] = { 0x00, 0xE0 };
	chip8_load(bytes, 2);

	int previousInstructionPointer = chip8_instruction_pointer;
	chip8_step();
	EXPECT_EQ(chip8_instruction_pointer, previousInstructionPointer + 2);
}

TEST(Chip8Interpreter, StepOnJumpMovesProgramCounter) {
	chip8_program_ptr program = chip8_assembler_init();
	chip8_init();

	chip8_jump(program, 0x789);

	chip8_load(chip8_get_program_start(program), chip8_get_program_size(program));

	chip8_step();
	EXPECT_EQ(chip8_instruction_pointer, 0x789);

	chip8_assembler_destroy(program);
}

TEST(Chip8Interpreter, StepOnJumpToZero) {
	chip8_init();

	// Jump 000
	uint8_t bytes[2] = { 0x10, 0x00 };
	chip8_load(bytes, 2);

	chip8_step();
	EXPECT_EQ(chip8_instruction_pointer, 0x000);
}

TEST(Chip8Interpreter, StepOnJumpToEnd) {
	chip8_init();

	// Jump 0xFFF
	uint8_t bytes[2] = { 0x1F, 0xFF };
	chip8_load(bytes, 2);

	chip8_step();
	EXPECT_EQ(chip8_instruction_pointer, 0xFFF);
}

TEST(Chip8Interpreter, StepOnRegisterAccumulator) {
	chip8_init();

	chip8_set_register_value(1, 17);
	chip8_set_register_value(2, 42);

	// V1 += V2
	uint8_t bytes[2] = { 0x81, 0x24 };
	chip8_load(bytes, 2);

	chip8_step();
	EXPECT_EQ(chip8_get_register_value_unsafe(1), 59);
}

TEST(Chip8Interpreter, StepOnRegisterAccumulatorWithOverflow) {
	chip8_init();

	chip8_set_register_value(1, 0xFF);
	chip8_set_register_value(2, 0x01);

	// V1 += V2
	uint8_t bytes[2] = { 0x81, 0x24 };
	chip8_load(bytes, 2);

	chip8_step();
	EXPECT_EQ(chip8_overflow_register, 1);
	EXPECT_EQ(chip8_get_register_value_unsafe(1), 0);
}

TEST(Chip8Interpreter, StepOnRegisterAccumulatorAdvancesInstructionPointer) {
	chip8_init();

	chip8_set_register_value(1, 0xFF);
	chip8_set_register_value(2, 0x01);

	// V1 += V2
	uint8_t bytes[2] = { 0x81, 0x24 };
	chip8_load(bytes, 2);

	int previousInstructionPointer = chip8_instruction_pointer;
	chip8_step();
	EXPECT_EQ(chip8_instruction_pointer, previousInstructionPointer + 2);
}

TEST(Chip8Interpreter, StepOnSpriteImpactsGraphicalMemory) {
	chip8_init();

	chip8_set_register_value(0, 0);
	chip8_index = CHIP8_PROGRAM_START + 2;

	// sprite v0 v0 1
	uint8_t bytes[3] = { 0xD0, 0x01, 0x55 };
	chip8_load(bytes, 3);

	chip8_step();

	assert_row(0, 0, 0, 1, 0, 1, 0, 1, 0, 1);
}

TEST(Chip8Interpreter, StepOnSpriteImpactsGraphicalMemoryWithManyRows) {
	chip8_init();

	chip8_set_register_value(0, 0);
	chip8_index = CHIP8_PROGRAM_START + 2;

	// sprite v0 v0 2
	uint8_t bytes[4] = { 0xD0, 0x02, 0x55, 0x7E };
	chip8_load(bytes, 4);

	chip8_step();

	assert_row(0, 1, 0, 1, 1, 1, 1, 1, 1, 0);
}

TEST(Chip8Interpreter, StepOnSpriteImpactsGraphicalMemoryInTheMiddle) {
	chip8_program_ptr program = chip8_assembler_init();
	chip8_init();

	int x_register_id = 5;
	int y_register_id = 6;

	chip8_set_register_value(x_register_id, 17);
	chip8_set_register_value(y_register_id, 10);
	chip8_index = CHIP8_PROGRAM_START + 2;

	chip8_sprite(program, x_register_id, y_register_id, 2);
	chip8_raw_data(program, 0x55);
	chip8_raw_data(program, 0x7E);
	chip8_load(chip8_get_program_start(program), chip8_get_program_size(program));

	chip8_step();

	assert_row(17, 11, 0, 1, 1, 1, 1, 1, 1, 0);

	chip8_assembler_destroy(program);
}
