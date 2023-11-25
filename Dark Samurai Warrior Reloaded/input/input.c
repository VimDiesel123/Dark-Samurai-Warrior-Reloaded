#include "./input.h"

void reset_input(Input *input) {
  input->downEndedDown = 0;
  input->leftEndedDown = 0;
  input->rightEndedDown = 0;
  input->upEndedDown = 0;
  input->tabEndedDown = 0;
}
