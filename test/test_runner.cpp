#include "unity.h"
#include <stdio.h>
#include "test_packet.hpp"
#include "test_kissesctelem.hpp"

void setUp(void)
{
  // set stuff up here
}

void tearDown(void)
{
  // clean stuff up here
}


/**
 * For native dev-platform or for some embedded frameworks
 */
int main(void)
{
  int retval = 0;
  retval += runUnityTests_packet();
  retval += runUnityTests_kissesctelem();
  return retval;
}