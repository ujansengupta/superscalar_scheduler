// Project3_ECE521.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <fstream>
#include <iomanip>
#include <string.h>
#include <sstream>
#include <vector>
#include <algorithm>


using namespace std;


int Cycle, PC, Width;
bool pipeLine_empty;

/*------------------------ Global Structures ------------------------*/

typedef struct RMT_line
{
	bool valid;
	int tag;
}RMTEntry;

typedef struct ROB_line
{
	int dest,pc;
	bool rdy;
	void clear()
	{
		dest = pc = 0;
		rdy = false;
	}
}ROBEntry;

typedef struct Instruction
{
	int pc, type, dst, rs1, rs2, count;
	bool rs1_rdy, rs2_rdy, rs1_ROB, rs2_ROB;
	int FE_start, FE_cycles, DE_start, DE_cycles, RN_start, RN_cycles, RR_start, RR_cycles, DI_start, DI_cycles;
	int IS_start, IS_cycles, EX_start, EX_cycles, WB_start, WB_cycles, RT_start, RT_cycles;
	int rs1_og, rs2_og;

	bool operator < (const Instruction &temp) const
	{
		return (pc < temp.pc);
	}
}Inst;

/*------------------------ Global Classes ------------------------*/

class Register
{
public:
	bool empty;
	vector <Inst> reg;
};

class IQClass
{
public:
	int IQ_Size;
	vector <Inst> iq;
};

class ROBClass
{
public:
	int head, tail;
	int ROB_Size;

	vector <ROBEntry> rob;
};

class RMTClass
{
public:
	int RMT_Size;
	RMTEntry rmt[67];
};

class E_list
{
public:
	vector <Inst> exec_list;
	void dec_count()
	{
		for (unsigned int i = 0; i < exec_list.size(); i++)
				exec_list[i].count--;
	}
	
};

/*------------------------ Global Objects ------------------------*/

Register DE, RN, RR, DI, WB, RT;
E_list Exec_List;
ROBClass ROB;
IQClass IQ;
RMTClass RMT;

/*------------------------ 	  Functions   ------------------------*/


bool Advance_Cycle(fstream& fin)
{
	if (pipeLine_empty == true && fin.eof())
		return false;
	else
		return true;
}

void calc_src_dst(Inst * instruction, string line)					//To parse the sources and destination//
{
	int dest = 0, dest_src1; 
	if (line[9] == '-')
		instruction->dst = -1;
	else if (line[10] == ' ')
	{
		instruction->dst = line[9] - '0';
		dest = 1;
	}
	else
		instruction->dst = atoi(line.substr(9, 2).c_str());

	//RS 1
	if (dest != 1)
	{
		if (line[12] == '-')
		{
			instruction->rs1 = instruction->rs1_og  = -1;
			dest_src1 = 4;
		}
		else if (line[13] == ' ')
		{
			instruction->rs1 = instruction->rs1_og = line[12] - '0';
			dest_src1 = 3;
		}
		else
		{
			instruction->rs1 = instruction->rs1_og = atoi(line.substr(12, 2).c_str());
			dest_src1 = 4;
		}
	}
	else
	{
		if (line[11] == '-')
		{
			instruction->rs1 = instruction->rs1_og  = -1;
			dest_src1 = 3;
		}
		else if (line[12] == ' ')
		{
			instruction->rs1 = instruction->rs1_og = line[11] - '0';
			dest_src1 = 2;
		}
		else
		{
			instruction->rs1 = instruction->rs1_og = atoi(line.substr(11, 2).c_str());
			dest_src1 = 3;
		}
	}

	//RS 2
	if (dest_src1 == 2)
		instruction->rs2 = instruction->rs2_og = atoi(line.substr(13, 2).c_str());
	else if (dest_src1 == 3)
		instruction->rs2 = instruction->rs2_og = atoi(line.substr(14, 2).c_str());
	else
		instruction->rs2 = instruction->rs2_og = atoi(line.substr(15, 2).c_str());
}

void Fetch(fstream& fin)
{
	string line;
	Inst instruction;
	int ctr = 0;

	if (fin.eof() || DE.empty == false)
		return;
	
	if (DE.empty)
	{
		while (getline(fin, line))
		{
			instruction.pc = PC;
			instruction.type = line[7] - '0';
			calc_src_dst(&instruction, line);
			instruction.rs1_rdy = instruction.rs2_rdy = instruction.rs1_ROB = instruction.rs2_ROB = false;
			instruction.FE_start = Cycle;
			instruction.FE_cycles = 1;
			instruction.DE_start = Cycle + 1;
			
			if (instruction.type == 0)
				instruction.count = 1;
			else if (instruction.type == 1)
				instruction.count = 2;
			else if (instruction.type == 2)
				instruction.count = 5;

			DE.reg.push_back(instruction);
			PC++;
			ctr++;
			
			if (DE.reg.size() == ((unsigned)Width))
				break;
		}
		if (ctr != 0)
			DE.empty = false;
	}
}

void Decode()
{
	if (!DE.empty)
	{
		if (RN.empty)
		{
			for (unsigned int i = 0; i < DE.reg.size(); i++)
			{
				DE.reg[i].RN_start = Cycle + 1;
				DE.reg[i].DE_cycles = DE.reg[i].RN_start - DE.reg[i].DE_start;
			}
			DE.reg.swap(RN.reg);
			DE.reg.clear();
			DE.empty = true;
			RN.empty = false;
		}
			
	}
}

void Rename()
{
	if (!RN.empty)
	{
		if (RR.empty)
		{
			//Calculating free space in the ROB//
			int ROBspace;
			if (ROB.tail < ROB.head)
				ROBspace = ROB.head - ROB.tail;
			else if (ROB.head < ROB.tail)
				ROBspace = ROB.ROB_Size - (ROB.tail - ROB.head);
			else
			{
				if (ROB.tail < (ROB.ROB_Size - 1))
				{
					if (ROB.rob[ROB.tail + 1].dest == 0 && ROB.rob[ROB.tail + 1].pc == 0 && ROB.rob[ROB.tail + 1].rdy == 0)
						ROBspace = ROB.ROB_Size;
					else
						ROBspace = 0;
				}
				else
				{
					if (ROB.rob[ROB.tail - 1].dest == 0 && ROB.rob[ROB.tail - 1].pc == 0 && ROB.rob[ROB.tail - 1].rdy == 0)
						ROBspace = ROB.ROB_Size;
					else
						ROBspace = 0;
				}
			}

			//Processing the bundle//

			if ((unsigned)ROBspace < RN.reg.size())
				return;
			else
			{
				for (unsigned int i = 0; i < RN.reg.size(); i++)
				{
					if (RN.reg[i].rs1 != -1)
						if (RMT.rmt[RN.reg[i].rs1].valid == true)							//If rs1 is not -1 and it has a valid RMT entry
						{
							RN.reg[i].rs1 = RMT.rmt[RN.reg[i].rs1].tag;						//Rename rs1
							RN.reg[i].rs1_ROB = true;										//Denotes that rs1 is from the ROB
						}


					if (RN.reg[i].rs2 != -1)
						if (RMT.rmt[RN.reg[i].rs2].valid == true)							//If rs2 is not -1 and it has a valid RMT entry
						{
							RN.reg[i].rs2 = RMT.rmt[RN.reg[i].rs2].tag;						//Rename rs2
							RN.reg[i].rs2_ROB = true;										//Denotes that rs2 is from the ROB
						}

					

					ROB.rob[ROB.tail].dest = RN.reg[i].dst;									//Set the ROB tail dst to the instruction dst
					ROB.rob[ROB.tail].pc = RN.reg[i].pc;									//Set the ROB tail pc to the instruction pc
					ROB.rob[ROB.tail].rdy = false;											//Set the ROB tail ready bit to false

					if (RN.reg[i].dst != -1)												//If the instruction has a destination (i.e. it's not -1)
					{
						RMT.rmt[RN.reg[i].dst].tag = ROB.tail;								//Assign the value of the current ROB tail to the RMT table
						RMT.rmt[RN.reg[i].dst].valid = true;								// entry for the dst register
					}

					RN.reg[i].dst = ROB.tail;												//Rename the destination

					if (ROB.tail != (ROB.ROB_Size - 1))
						ROB.tail++;
					else
						ROB.tail = 0;

					RN.reg[i].RR_start = Cycle + 1;
					RN.reg[i].RN_cycles = RN.reg[i].RR_start - RN.reg[i].RN_start;
				}
				RN.reg.swap(RR.reg);
				RN.reg.clear();
				RN.empty = true;
				RR.empty = false;
			}
		}

	}
}

void RegRead()
{
	if (!RR.empty)
	{
		if (DI.empty)
		{
			for (unsigned int i = 0; i < RR.reg.size(); i++)
			{
				if (RR.reg[i].rs1_ROB)
				{
					if (ROB.rob[RR.reg[i].rs1].rdy == 1)
						RR.reg[i].rs1_rdy = true;
				}
				else
					RR.reg[i].rs1_rdy = true;

				if (RR.reg[i].rs2_ROB)
				{
					if (ROB.rob[RR.reg[i].rs2].rdy == 1)
						RR.reg[i].rs2_rdy = true;
				}
				else
					RR.reg[i].rs2_rdy = true;

				RR.reg[i].DI_start = Cycle + 1;
				RR.reg[i].RR_cycles = RR.reg[i].DI_start - RR.reg[i].RR_start;
			}
			RR.reg.swap(DI.reg);
			RR.reg.clear();
			RR.empty = true;
			DI.empty = false;
		}
	}
}

void Dispatch()
{
	if (!DI.empty)
	{
		if ((IQ.IQ_Size - IQ.iq.size()) >= DI.reg.size())
		{
			for (unsigned int i = 0; i < DI.reg.size(); i++)
			{
				DI.reg[i].IS_start = Cycle + 1;
				DI.reg[i].DI_cycles = DI.reg[i].IS_start - DI.reg[i].DI_start;
				IQ.iq.push_back(DI.reg[i]);
			}
			DI.reg.clear();
			DI.empty = true;
		}
	}
}

void Issue()
{
	if (IQ.iq.size() != 0)
	{
		sort(IQ.iq.begin(), IQ.iq.end());

		int i = 0;

		int ctr = 1;
		while (ctr != 0)
		{
			ctr = 0;
			for (unsigned int j = 0; j < IQ.iq.size(); j++)
			{
				if (IQ.iq[j].rs1_rdy && IQ.iq[j].rs2_rdy)
				{
					IQ.iq[j].EX_start = Cycle + 1;
					IQ.iq[j].IS_cycles = IQ.iq[j].EX_start - IQ.iq[j].IS_start;
					Exec_List.exec_list.push_back(IQ.iq[j]);
					IQ.iq.erase(IQ.iq.begin() + j);
					i++;
					ctr++;
					break;
				}
			}
			if (i == Width)
				break;
		}
		
	}
	
}

void Execute()
{
	if (Exec_List.exec_list.size() != 0)
	{
		Exec_List.dec_count();

		int ctr = 1;
		
		while (ctr != 0)
		{
			ctr = 0;

			for (unsigned int i = 0; i < Exec_List.exec_list.size(); i++)
			{
				if (Exec_List.exec_list[i].count == 0)
				{
					Exec_List.exec_list[i].WB_start = Cycle + 1;
					Exec_List.exec_list[i].EX_cycles = Exec_List.exec_list[i].WB_start - Exec_List.exec_list[i].EX_start;

					WB.reg.push_back(Exec_List.exec_list[i]);

					for (unsigned int x = 0; x < IQ.iq.size(); x++)										//Wake up dependent instructions in the IQ
					{
						if (IQ.iq[x].rs1 == Exec_List.exec_list[i].dst)
							IQ.iq[x].rs1_rdy = true;

						if (IQ.iq[x].rs2 == Exec_List.exec_list[i].dst)
							IQ.iq[x].rs2_rdy = true;
					}

					for (unsigned int y = 0; y < DI.reg.size(); y++)									//Wake up dependent instructions in the DI
					{
						if (DI.reg[y].rs1 == Exec_List.exec_list[i].dst)
							DI.reg[y].rs1_rdy = true;

						if (DI.reg[y].rs2 == Exec_List.exec_list[i].dst)
							DI.reg[y].rs2_rdy = true;
					}

					for (unsigned int z = 0; z < RR.reg.size(); z++)									//Wake up dependent instructions in the RR
					{
						if (RR.reg[z].rs1 == Exec_List.exec_list[i].dst)
							RR.reg[z].rs1_rdy = true;

						if (RR.reg[z].rs2 == Exec_List.exec_list[i].dst)
							RR.reg[z].rs2_rdy = true;
					}

					Exec_List.exec_list.erase(Exec_List.exec_list.begin() + i);
					ctr++;
					break;
				}
			}
		}

		
	}
}

void Writeback()
{
	if (WB.reg.size() != 0)
	{
		for (unsigned int i = 0; i < WB.reg.size(); i++)
		{
			ROB.rob[WB.reg[i].dst].rdy = true;
			WB.reg[i].RT_start = Cycle + 1;
			WB.reg[i].WB_cycles = WB.reg[i].RT_start - WB.reg[i].WB_start; 
			RT.reg.push_back(WB.reg[i]);
		}
		
		WB.reg.clear();
	}
}

void Retire()
{
	int i = 0;

	while ((i < Width))
	{
		if (ROB.head == ROB.tail && ROB.head != ROB.ROB_Size - 1)
		{
			if (ROB.rob[ROB.head + 1].pc == 0)
				return;
		}

		if (ROB.rob[ROB.head].rdy)
		{
			for (unsigned int j = 0; j < RR.reg.size(); j++)
			{
				if (RR.reg[j].rs1 == ROB.head)
					RR.reg[j].rs1_rdy = true;

				if (RR.reg[j].rs2 == ROB.head)
					RR.reg[j].rs2_rdy = true;
			}

			for (unsigned int x = 0; x < RT.reg.size(); x++)
			{
				if (RT.reg[x].pc == ROB.rob[ROB.head].pc)
				{
					RT.reg[x].RT_cycles = (Cycle + 1) - RT.reg[x].RT_start;

					cout << RT.reg[x].pc << " fu{" << RT.reg[x].type << "} src{" << RT.reg[x].rs1_og << "," << RT.reg[x].rs2_og << "} ";
					cout << "dst{" << ROB.rob[ROB.head].dest << "} FE{" << RT.reg[x].FE_start << "," << RT.reg[x].FE_cycles << "} ";
					cout << "DE{" << RT.reg[x].DE_start << "," << RT.reg[x].DE_cycles << "} RN{" << RT.reg[x].RN_start << "," << RT.reg[x].RN_cycles << "} ";
					cout << "RR{" << RT.reg[x].RR_start << "," << RT.reg[x].RR_cycles << "} DI{" << RT.reg[x].DI_start << "," << RT.reg[x].DI_cycles << "} ";
					cout << "IS{" << RT.reg[x].IS_start << "," << RT.reg[x].IS_cycles << "} EX{" << RT.reg[x].EX_start << "," << RT.reg[x].EX_cycles << "} ";
					cout << "WB{" << RT.reg[x].WB_start << "," << RT.reg[x].WB_cycles << "} RT{" << RT.reg[x].RT_start << "," << RT.reg[x].RT_cycles << "} " << endl;

					RT.reg.erase(RT.reg.begin() + x);
					break;
				}

			}


			for (int z = 0; z < RMT.RMT_Size; z++)
			{
				if (RMT.rmt[z].tag == ROB.head)
				{
					RMT.rmt[z].tag = 0;
					RMT.rmt[z].valid = false;
				}
			}
			ROB.rob[ROB.head].clear();

			if (ROB.head != (ROB.ROB_Size - 1))
				ROB.head++;
			else
				ROB.head = 0;

			i++;
		}
		else
			break;
	}
}


int main(int argc, char* argv[])
{
	int ROB_Size = atoi(argv[1]);
	int IQ_Size = atoi(argv[2]);
	Width = atoi(argv[3]);
	char *trace = (char*)malloc(20);
	trace = argv[4];

	Cycle = 0;
	PC = 0;
	pipeLine_empty = false;

	/*---------------------  INITIALIZATION   --------------------------------*/
	ROB.ROB_Size = ROB_Size;
	IQ.IQ_Size = IQ_Size;

	DE.empty = RN.empty = RR.empty = DI.empty = WB.empty = RT.empty = true;
	RMT.RMT_Size = 67;

	ROB.head = ROB.tail = 3;

	for (int i = 0; i < 67; i++)
	{
		RMT.rmt[i].valid = false;
		RMT.rmt[i].tag = -1;
	}

	ROBEntry rb;													//Initializing the ROB
	rb.clear();
	for (int i = 0; i < ROB_Size; i++)
		ROB.rob.push_back(rb);

	/*---------------------------------------------------------------------------*/

	fstream fin;
	FILE * pFile;

	pFile = fopen(trace, "r");

	fin.open(trace, ios::in);

	while (Advance_Cycle(fin))
	{
		Retire();
		Writeback();
		Execute();
		Issue();
		Dispatch();
		RegRead();
		Rename();
		Decode();
		Fetch(fin);
		
		if (DE.empty && RN.empty && RR.empty && DI.empty && IQ.iq.size() == 0 && Exec_List.exec_list.size() == 0 && WB.reg.size() == 0)
			if (ROB.head == ROB.tail)
				if (ROB.rob[ROB.tail].pc == 0)
					pipeLine_empty = true;

		Cycle++;
		
	}

	fclose(pFile);

	cout << "# === Simulator Command =========" << endl;
	cout << "# ";
	for (int i = 0; i < argc; i++)
		cout << argv[i] << " ";
	cout << endl << "# === Processor Configuration ===" << endl;
	cout << "# ROB_SIZE = " << ROB_Size << endl;
	cout << "# IQ_SIZE  = " << IQ_Size << endl;
	cout << "# WIDTH    = " << Width << endl;
	cout << "# === Simulation Results ========" << endl;
	cout << "# Dynamic Instruction Count    = " << PC << endl;
	cout << "# Cycles                       = " << Cycle << endl;
	cout << "# Instructions Per Cycle (IPC)  = " << fixed << setprecision(2) << ((float)PC / (float)Cycle) << endl;
}
