static unsigned long next = 1;

int rand(void)
{
    next = next * 1103515245UL + 12345UL;
    return (unsigned int)(next / 65536UL) % 32768UL;
}

void srand(unsigned int seed)
{
    next = seed;
}
