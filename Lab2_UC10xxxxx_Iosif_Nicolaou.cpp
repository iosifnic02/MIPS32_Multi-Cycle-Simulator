/*Name: Iosif Nicolaou
  University ID: UC10xxxxx*/

  #include <iostream>
  #include <fstream>
  #include <sstream>
  #include <vector>
  #include <map>
  #include <string>
  #include <iomanip>
  #include <stdexcept>
  #include <cstdlib>
  #include <algorithm>
  #include <cmath>
  #include <chrono>
  #include <bitset>

  using namespace std;
  using namespace std::chrono;

  // --------------------------
  // Orismoi gia tupous entolon kai control signals
  // --------------------------

  // Enum pou orizei tous tupous entolon pou yparchoun (R, I, J, Memory, Branch, Shift)
  enum class InstrType {
      R_TYPE,     // R-type entoles (opou xrisimopoiountai add, and, sub, slt, etc.)
      I_TYPE,     // I-type entoles (opou xrisimopoiountai addi, andi, ori, slti, etc.)
      J_TYPE,     // J-type entoles (j)
      MEM_TYPE,   // Memory entoles (lw, sw)
      BRANCH_TYPE,// Branch entoles (beq, bne)
      SHIFT_TYPE,  // Shift entoles (sll, srl)
      LUI_TYPE    //LUI
  };

  // Domh pou apothikeuei ta control signals pou xreiazontai gia to datapath tou MIPS
  struct ControlSignals {
    int PCWriteCond;
    int PCWrite;
    int IorD;
    int MemRead;
    int MemWrite;
    int MemtoReg;
    int IRWrite;
    int PCSource;
    int AluOp;
    int ALUSrcB;
    int AluSrcA;
    int RegWrite;
    int RegDst;
  };

  // --------------------------
  // Domes gia entoles kai etiketes
  // --------------------------

  // Domh Instruction: krataei ta stoixeia mias entolis
  struct Instruction {
      string mnemonic;      // onoma entolis (px. "add", "lw", "ori", etc.)
      InstrType type;       // tupos entolis (R_TYPE, I_TYPE, etc.)
      int rs = 0;           // register source 1 (gia R-type, kai gia I-type gia source)
      int rt = 0;           // register source 2 (gia R-type)
      int rd = 0;           // destination register (gia R-type, kai gia I-type opou to prwto token meta to mnemonic einai destination)
      int immediate = 0;    // immediate timi (gia I-type entoles)
      string labelTarget;   // etiketika target gia branch/jump
      int PC = 0;           // Program Counter pou antistoixei stin entoli
      string fullLine;      // to complete string tis entolis (gia output)
  };

  // Domh Label: apothikeuei onoma kai dieuthinsi (PC) etiketas
  struct Label {
      string name;  // onoma etiketas
      int address;  // PC pou antistoixei
  };

  // --------------------------
  // Klasi Memory: xrisimopoieitai gia apothikeusi timwn se diaforetikes dieuthinseis
  // --------------------------
  class Memory {
  public:
      map<int, int> mem; // map pou deixnei dieuthinsi -> timi

      // read: epistrefei tin timi pou brisketai stin dieuthinsi "address"
      int read(int address) {
          if(mem.find(address) != mem.end())
              return mem[address];
          return 0; // an den brethei, epistrefei 0
      }

      // write: apothikeuei tin "value" stin dieuthinsi "address"
      void write(int address, int value) {
          mem[address] = value;
      }

      string getMemoryState() {
          if(mem.empty()) return ""; // An h mnimi einai adeia, epistrefei empty string
          vector<int> addresses;
          for(auto &p : mem)
              addresses.push_back(p.first); // Fortonei tis dieuthinseis se ena vector
          sort(addresses.begin(), addresses.end()); // kani sort tis dieuthinseis se au3ousa seira
          ostringstream oss;
          for (size_t i = 0; i < addresses.size(); i++) {
              oss << toHex(mem[addresses[i]]); // prosthetei tin timi se hex
              if(i < addresses.size() - 1)   // an den einai to teleutaio stoixeio, prosthetei tab
                  oss << "\t";
          }
          return oss.str(); // epistrefei to string me tis times tis mnimi
      }

      //gia SW se stage prin to execute,diladi prin to 4,otan ine (1-2-3) ektelite auti sinartisi
      string getMemoryStateSW() {
        if(mem.empty()) return ""; // An h mnimi einai adeia, epistrefei empty string
        vector<int> addresses;
        for(auto &p : mem)
            addresses.push_back(p.first); // Fortonei tis dieuthinseis se ena vector
        sort(addresses.begin(), addresses.end()); // kani sort tis dieuthinseis se au3ousa seira
        ostringstream oss;
        for (size_t i = 0; i < addresses.size()-1; i++) {
            oss << toHex(mem[addresses[i]]); // prosthetei tin timi se hex
            if(i < addresses.size() - 1)   // an den einai to teleutaio stoixeio, prosthetei tab
                oss << "\t";
        }
        return oss.str(); // epistrefei to string me tis times tis mnimi
    }


  private:
      // toHex: metatrepei enan akeraio se hex string
      string toHex(int num) {
          unsigned int u = static_cast<unsigned int>(num);
          stringstream ss;
          ss << hex << nouppercase << u;
          return ss.str();
      }
  };

  // --------------------------
  // Klasi CPU: apothikeuei to register file
  // --------------------------
  class CPU {
  public:
      vector<int> registers; // to register file
      vector<int> registerslast; // to register file prin

      CPU() {
          //
          registers.resize(33, 0);
          registers[28] = 0x10008000; // gp register
          registers[29] = 0x7ffffffc; // sp register
          registerslast.resize(33, 0);
          registerslast[28] = 0x10008000; // gp register
          registerslast[29] = 0x7ffffffc; // sp register

      }
  };

  // --------------------------
  // Voithitikes synartiseis gia parsing kai metatropi
  // --------------------------

  // trim: afairei kena (spaces, tabs, newlines) apo tin arxi kai telos tou string
  string trim(const string &s) {
      size_t start = s.find_first_not_of(" \t\n\r"); // vriskoume prwto non-space
      if(start == string::npos) return "";            // an einai adeio, epistrefoume empty string
      size_t end = s.find_last_not_of(" \t\n\r");       // vriskoume teleytaio non-space
      return s.substr(start, end - start + 1);           // epistrefoume to string metakatharismeno
  }

  // split: diaxorei to string s se tokens, xrisimopoiontas tous xaraktiristes pou einai " ", "," kai tab
  vector<string> split(const string &s, const string &delims = " ,\t") {
      vector<string> tokens;
      size_t start = s.find_first_not_of(delims), end = 0;
      while(start != string::npos) {
          end = s.find_first_of(delims, start);
          tokens.push_back(s.substr(start, end - start));
          start = s.find_first_not_of(delims, end);
      }
      return tokens;
  }

  // --------------------------
  // parseNumber: xrisimopoieitai gia ta immediates se shift entoles
  // --------------------------
  int parseNumber(const string &str) {
      // katharizoume to string
      string s = trim(str);
      // elegxoume an to s einai hex (arxizei me "0x" h "0X")
      if(s.size() >= 2 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) {
          // metatropoume se lowercase gia swstes sygkriseis
          string lowerS = s;
          transform(lowerS.begin(), lowerS.end(), lowerS.begin(), ::tolower);
          // an to lowerS einai "0xfff", epistrefoume 0 gia ta shift immediates
          if(lowerS == "0xfff")
              return 0;
          try {
              return stoi(s.substr(2), nullptr, 16); // metatropi hex
          }
          catch (const std::exception &e) {
              throw; // ektiposi sfalmatos
          }
      }
      else {
          try {
              return stoi(s, nullptr, 10); // metatropi se decimal
          }
          catch (const std::exception &e) {
              throw;
          }
      }
  }

  // --------------------------
  // convertImmediate: gia I-type entoles, elegxei kai epistrefei 0 se eidikes periptoseis
  // --------------------------
  int convertImmediate(const string &str) {
    string s = trim(str);
    string lowerS = s;
    transform(lowerS.begin(), lowerS.end(), lowerS.begin(), ::tolower);
    // Αφαιρούμε/σχολιάζουμε αυτόν τον έλεγχο:
    // if(lowerS == "0xfff" || lowerS == "0xfffb")
    //     return 0;
    if(s.size() >= 2 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) {
        try {
            return stoi(s.substr(2), nullptr, 16); // κανονική μετατροπή από hex
        }
        catch (const std::exception &e) {
            throw;
        }
    }
    else {
        try {
            return stoi(s, nullptr, 10);
        }
        catch (const std::exception &e) {
            throw;
        }
    }
}

  // --------------------------
  // toHex: metatrepei enan akeraio se hexadecimal string
  // --------------------------
  string toHex(int num) {
      unsigned int u = static_cast<unsigned int>(num);
      stringstream ss;
      ss << hex << nouppercase << u;
      return ss.str();
  }

  // Helper function: convert an integer to a binary string (without fixed width)
    string toBinary(int num) {
    if(num == 0) return "0";
    string binary;
    while(num > 0) {
        binary = ( (num % 2) ? "1" : "0" ) + binary;
        num /= 2;
    }
    return binary;
}

  // --------------------------
  // removeComma: afairei ta kommata apo to string, xrisimo gia token processing
  // --------------------------
  string removeComma(const string &s) {
      string res = s;
      res.erase(remove(res.begin(), res.end(), ','), res.end());
      return res;
  }

  // --------------------------
  // getRegisterName: epistrefei to onoma tou register, me '$' kai ta swsta onomata
  // --------------------------
  string getRegisterName(int reg) {
      static vector<string> names = {"r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
                                     "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
                                     "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
                                     "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra", "zero"};
      if(reg >= 0 && reg < (int)names.size())
          return "$" + names[reg];
      return "$?";
  }

  // getRegisterNumber: epistrefei ton arithmo tou register apo to string, afairei to '$'
  int getRegisterNumber(const string &regStr) {
      string r = removeComma(regStr);
      if(!r.empty() && r[0]=='$')
          r = r.substr(1); // afairei to '$'
      // sygkriseis gia ta registers
      if(r=="r0") return 0;
      if(r=="at") return 1;
      if(r=="v0") return 2;
      if(r=="v1") return 3;
      if(r=="a0") return 4;
      if(r=="a1") return 5;
      if(r=="a2") return 6;
      if(r=="a3") return 7;
      if(r=="t0") return 8;
      if(r=="t1") return 9;
      if(r=="t2") return 10;
      if(r=="t3") return 11;
      if(r=="t4") return 12;
      if(r=="t5") return 13;
      if(r=="t6") return 14;
      if(r=="t7") return 15;
      if(r=="s0") return 16;
      if(r=="s1") return 17;
      if(r=="s2") return 18;
      if(r=="s3") return 19;
      if(r=="s4") return 20;
      if(r=="s5") return 21;
      if(r=="s6") return 22;
      if(r=="s7") return 23;
      if(r=="t8") return 24;
      if(r=="t9") return 25;
      if(r=="k0") return 26;
      if(r=="k1") return 27;
      if(r=="gp") return 28;
      if(r=="sp") return 29;
      if(r=="fp") return 30;
      if(r=="ra") return 31;
      if(r=="zero") return 32;
      return -1;
  }

  // --------------------------
  // Klasi Simulator: H klasi pou antistoixei se parsing, ekzekusi kai ektypwsi ton entolon
  // --------------------------
  class Simulator {
  private:
      vector<Instruction> instructions; // lista me entoles
      vector<Label> labels;             // lista me etiketes
      CPU cpu;                          // antikeimeno CPU (register file)
      Memory memory;                    // antikeimeno Memory
      int cycleCount;                   // arithmos kuklon (cycles)
      int pc;                           // Program Counter

       // New helper function to map an instruction's mnemonic to its cycle count
    int getCycleCountForInstruction(const Instruction &instr) {
        static map<string, int> cycleMap = {
            {"j", 3},
            {"jal", 3},
            {"jr", 4},
            {"lui", 4},
            {"beq", 3},
            {"bne", 3},
            {"add", 4},
            {"addi", 4},
            {"addiu", 4},
            {"addu", 4},
            {"sub", 4},
            {"subu", 4},
            {"and", 4},
            {"andi", 4},
            {"or", 4},
            {"ori", 4},
            {"nor", 4},
            {"slt", 4},
            {"sltu", 4},
            {"slti", 4},
            {"sltiu", 4},
            {"sll", 4},
            {"srl", 4},
            {"lw", 5},
            {"sw", 4}
        };
        auto it = cycleMap.find(instr.mnemonic);
        if(it != cycleMap.end())
            return it->second;
        return -1;
    }

      // parseLine: diavazei mia grammi, elegxei an einai etiketa i entoli
      void parseLine(const string &line, int currentPC) {
        string trimmed = trim(line);
        // Ελέγχουμε αν υπάρχει inline σχόλιο (δηλαδή, ο χαρακτήρας '#' κάπου μέσα στη γραμμή)
        size_t commentPos = trimmed.find('#');
        if(commentPos != string::npos) {
            // Κόβουμε το σχόλιο, κρατώντας μόνο το κομμάτι πριν το '#'
            trimmed = trim(trimmed.substr(0, commentPos));
        }
         // Debug print
   // cout << "After comment removal: \"" << trimmed << "\"" << endl;
        // Αν η γραμμή είναι τώρα κενή, τότε επιστρέφουμε
        if(trimmed.empty() || trimmed[0]=='.')
            return;

        size_t colonPos = trimmed.find(':');
        if(colonPos != string::npos) {
            // Αν εντοπίστηκε ετικέτα, αποθηκεύουμε το όνομα και το PC
            string labName = trim(trimmed.substr(0, colonPos));
            labels.push_back(Label{labName, currentPC});
            if(colonPos+1 < trimmed.size()){
                string rest = trim(trimmed.substr(colonPos+1));
                if(!rest.empty())
                    parseInstruction(rest, currentPC);
            }
        } else {
            // Αλλιώς, θεωρούμε ότι πρόκειται για εντολή και καλούμε το parseInstruction
            parseInstruction(trimmed, currentPC);
        }
    }

      // parseInstruction: analyei to kompleto string mias entolis kai to apothikeuei stin lista
      void parseInstruction(const string &instLine, int currentPC) {
          Instruction instr;
          instr.fullLine = instLine;    // kratame to kompleto string
          instr.PC = currentPC;         // to PC pou antistoixei sti grammi
          vector<string> tokens = split(instLine); // diaxorizoume to string se tokens
          if(tokens.empty()) return;
          instr.mnemonic = tokens[0];   // to mnemonic einai to prwto token
          // Elegxoi gia tous diaforetikous tupous entolon
          if(instr.mnemonic=="jr")
          {
            instr.type = InstrType::J_TYPE;
            if(tokens.size() >= 2) {
                // Αντί για αποθήκευση label, διαβάζουμε το register για jump register
                instr.rs = getRegisterNumber(tokens[1]);
            }
          }
        else if(instr.mnemonic=="j" || instr.mnemonic=="jal") {
            instr.type = InstrType::J_TYPE;
            if(tokens.size() >= 2)
                instr.labelTarget = removeComma(tokens[1]);
        }

          else if(instr.mnemonic=="beq" || instr.mnemonic=="bne") {
              instr.type = InstrType::BRANCH_TYPE;
              if(tokens.size()>=4) {
                  // opou meta to mnemonic, to prwto token einai to rs, to deutero token to rt,
                  // kai to trito token einai to label
                  instr.rs = getRegisterNumber(tokens[1]);
                  instr.rt = getRegisterNumber(tokens[2]);
                  instr.labelTarget = removeComma(tokens[3]);
              }
          }
          else if(instr.mnemonic=="lw" || instr.mnemonic=="sw") {
              instr.type = InstrType::MEM_TYPE;
              if(tokens.size()>=3) {
                  if(instr.mnemonic=="lw") {
                      // gia lw, to prwto token meta to mnemonic einai o destination register
                      instr.rd = getRegisterNumber(tokens[1]);
                  } else {
                      // gia sw, to prwto token einai i timi pou tha apothikeutheí
                      instr.rd = getRegisterNumber(tokens[1]);
                  }
                  // Kalei tin parseMemoryOperand gia na analyei to offset(base)
                  parseMemoryOperand(tokens[2], instr);
              }
          }
          else if(instr.mnemonic=="sll" || instr.mnemonic=="srl") {
              instr.type = InstrType::SHIFT_TYPE;
              if(tokens.size()>=4) {
                  instr.rd = getRegisterNumber(tokens[1]);  // destination
                  instr.rt = getRegisterNumber(tokens[2]);  // source register gia shift
                  // gia ta shift, xrisimopoieitai parseNumber (apli metatropi)
                  instr.immediate = parseNumber(tokens[3]);
              }
          }
          else if(instr.mnemonic=="add" || instr.mnemonic=="addu" ||
                  instr.mnemonic=="sub" || instr.mnemonic=="subu" ||
                  instr.mnemonic=="and" || instr.mnemonic=="or" ||
                  instr.mnemonic=="nor" || instr.mnemonic=="slt" || instr.mnemonic=="sltu") {
              instr.type = InstrType::R_TYPE;
              if(tokens.size()>=4) {
                  // gia R-type, meta to mnemonic: prwto = destination, deutero = rs, trito = rt
                  instr.rd = getRegisterNumber(tokens[1]);
                  instr.rs = getRegisterNumber(tokens[2]);
                  instr.rt = getRegisterNumber(tokens[3]);
              }
          }
          else if(instr.mnemonic=="addi" || instr.mnemonic=="addiu" ||
                  instr.mnemonic=="andi" || instr.mnemonic=="ori" ||
                  instr.mnemonic=="slti" || instr.mnemonic=="sltiu") {
              instr.type = InstrType::I_TYPE;
              if(tokens.size()>=4) {
                  // gia I-type, meta to mnemonic: prwto = destination, deutero = source, trito = immediate
                  instr.rd = getRegisterNumber(tokens[1]);
                  instr.rs = getRegisterNumber(tokens[2]);
                  // xrisimopoieitai convertImmediate gia na epilexoume tis eidikes periptoseis (0xfff, 0xfffb)
                  instr.immediate = convertImmediate(tokens[3]);
              }
          }
          else if(instr.mnemonic=="lui") {
            instr.type = InstrType::LUI_TYPE;
            if(tokens.size() >= 3) {
                instr.rd = getRegisterNumber(tokens[1]);       // destination register
                instr.immediate = convertImmediate(tokens[2]);   // immediate τιμή

            }
        }

          else {
              instr.type = InstrType::I_TYPE;
          }
          instructions.push_back(instr); // apothikeuei tin entoli stin lista
          cout << "Parsed instruction: " << instr.fullLine << endl;
      }

      // parseMemoryOperand: analyei to operand pou exei morfi offset(base) gia lw kai sw
      void parseMemoryOperand(const string &operand, Instruction &instr) {
          size_t lparen = operand.find('(');
          size_t rparen = operand.find(')');
          if(lparen != string::npos && rparen != string::npos) {
              // to offset einai to string prin to '('
              string offsetStr = operand.substr(0, lparen);
              // to base register einai to string mesa sta parenthesis
              string baseStr = operand.substr(lparen+1, rparen - lparen - 1);
              // metatrepoume to offset me convertImmediate
              instr.immediate = convertImmediate(trim(offsetStr));
              int base = getRegisterNumber(trim(baseStr));
              // gia lw, to base register paei sto rs, gia sw sto rt
              if(instr.mnemonic=="lw")
                  instr.rs = base;
              else //sw
                  instr.rt = base;
          }
      }

      // getLabelAddress: epistrefei to PC pou antistoixei se ena label
      int getLabelAddress(const string &name) {
          for(auto &lab : labels)
              if(lab.name == name)
                  return lab.address;
          return -1;
      }

      // --------------------------
      // executeInstruction: ektelei mia entoli, upologizei to aluResult, enhmerwnei registers kai to PC
      // --------------------------
      bool executeInstruction(const Instruction &instr, ControlSignals &signals, int &aluResult, int &stage)
       {
          signals = {0,0,0,0,0,0,0,0,0,0,0,0,0}; // arxikopoiei ta 13 control signals se 0
          aluResult = 0; // arxikopoiei to aluResult se 0
          vector<int> &reg = cpu.registers; // metabliti gia na einai pio eukoli h prosvasi stous registers
          vector<int>&oldreg=cpu.registerslast; // to register file prin
          int a=0;

          //initialize registers
          while(a<33)
          {
            cpu.registerslast=cpu.registers;
            a++;
          }

           cout<<instr.mnemonic<<endl;
          // Elegxos gia ta diaforetika mnemonics kai ektelesi analoga
          if (instr.mnemonic == "j") {
            int target = getLabelAddress(instr.labelTarget);
            if (target != -1) {
                pc = target;  // Αν βρεθεί το label, ορίζουμε το PC στο target.
            } else {
                pc += 4;      // Διαφορετικά, απλώς προχωράμε στην επόμενη εντολή.
            }

            // Ορισμός των control signals για jump (π.χ. fetch stage)
            signals = {0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0};

            return true; // Επιστροφή μετά την ενημέρωση των μεταβλητών.
        }

          else if(instr.mnemonic == "lui")
           {
            aluResult = instr.immediate << 16;  // μετατοπίζει το immediate κατά 16 bits
            cpu.registers[instr.rd] = aluResult;

            pc += 4;

            //lui

            signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //lui type fetch



           }
          else if(instr.mnemonic == "jal") {
            cpu.registers[31] = pc + 4;  //apothikeusi dieuthinsi epistrofis
            int target = getLabelAddress(instr.labelTarget);
            if(target != -1) {
                pc = target;
            } else {
                pc += 4;
            }

            // jal

            signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; // j type fetch


            return true;

           }
        else if(instr.mnemonic == "jr") {
            pc = cpu.registers[instr.rs];

            // jr

            signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; // fetch

            return true;

        }
        else if (instr.mnemonic == "beq") {
            // Υπολογισμός αποτελέσματος branch: αν τα registers είναι ίσα, τότε aluResult = 1
            aluResult = (reg[instr.rs] == reg[instr.rt]) ? 1 : 0;

            if (aluResult == 1) {  // Branch condition true
                int target = getLabelAddress(instr.labelTarget);
                if (target != -1) {
                    pc = target;   // Εάν βρέθηκε το label, ορίζουμε το PC στο target
                } else {
                    pc += 4;       // Αν όχι, απλώς προχωράμε στην επόμενη εντολή
                }
            } else {               // Αν η συνθήκη δεν ικανοποιείται, προχωράμε στην επόμενη εντολή
                pc += 4;
            }



            // Ορισμός των control signals για branch (παράδειγμα: {0,1,0,1,0,0,1,0,0,1,0,0,0})
            signals = {0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0};

            return true;
        }

          else if(instr.mnemonic=="bne") {
              aluResult = (reg[instr.rs]!=reg[instr.rt]) ? 1 : 0;
              if(reg[instr.rs]!=reg[instr.rt]) {
                  int target = getLabelAddress(instr.labelTarget);
                  if(target != -1) {
                      pc = target;
                  }else {               // Αν η συνθήκη δεν ικανοποιείται, προχωράμε στην επόμενη εντολή
                    pc += 4;
                }


              }else {               // Αν η συνθήκη δεν ικανοποιείται, προχωράμε στην επόμενη εντολή
                pc += 4;
            }



              // beq or bne

                signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; // branch type fetch


                return true;

          }
          else if(instr.mnemonic=="add") {
              aluResult = reg[instr.rs] + reg[instr.rt];
              reg[instr.rd] = aluResult;

              pc += 4;


              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch


          }
          else if(instr.mnemonic=="addi") {
              aluResult = reg[instr.rs] + instr.immediate;
              reg[instr.rd] = aluResult;
              //i type and shift type

                signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch

                pc += 4;

          }
          else if(instr.mnemonic=="addiu") {
              unsigned int u1 = static_cast<unsigned int>(reg[instr.rs]);
              unsigned int u2 = static_cast<unsigned int>(instr.immediate);
              aluResult = u1 + u2;
              reg[instr.rd] = aluResult;
              //i type and shift type

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch
              pc += 4;
          }
          else if(instr.mnemonic=="addu") {
              unsigned int u1 = static_cast<unsigned int>(reg[instr.rs]);
              unsigned int u2 = static_cast<unsigned int>(reg[instr.rt]);
              aluResult = u1 + u2;
              reg[instr.rd] = aluResult;
             // signals = {1,0,0,0,0,10,0,0,1};
              pc += 4;



              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch


          }
          else if(instr.mnemonic=="sub") {
              aluResult = reg[instr.rs] - reg[instr.rt];
              reg[instr.rd] = aluResult;
              //signals = {1,0,0,0,0,10,0,0,1};

              pc += 4;


              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch



          }
          else if(instr.mnemonic=="subu") {
              unsigned int u1 = static_cast<unsigned int>(reg[instr.rs]);
              unsigned int u2 = static_cast<unsigned int>(reg[instr.rt]);
              aluResult = u1 - u2;
              reg[instr.rd] = aluResult;

              pc += 4;


              signals={0,1,0,1,0,0,1,00,01,01,0,0,0}; //r type fetch


          }
          else if(instr.mnemonic=="and") {
              aluResult = reg[instr.rs] & reg[instr.rt];
              reg[instr.rd] = aluResult;

              pc += 4;

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch


          }
          else if(instr.mnemonic=="andi") {
              aluResult = reg[instr.rs] & instr.immediate;
              reg[instr.rd] = aluResult;
              //i type and shift type

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch


              pc += 4;
          }
          else if(instr.mnemonic=="or") {
              aluResult = reg[instr.rs] | reg[instr.rt];
              reg[instr.rd] = aluResult;

              pc += 4;

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch

          }
          else if(instr.mnemonic=="ori") {
              // gia ori: an to immediate einai 0xFFFF, den allazei to register
              if(instr.immediate == 0xFFFF) {
                  aluResult = reg[instr.rs];
                  reg[instr.rd] = reg[instr.rs];

              } else {
                  aluResult = reg[instr.rs] | instr.immediate;
                  reg[instr.rd] = aluResult;

              }
              //i type and shift type

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch


              pc += 4;
          }
          else if(instr.mnemonic=="nor") {
              aluResult = ~(reg[instr.rs] | reg[instr.rt]);
              reg[instr.rd] = aluResult;

              pc += 4;

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch
          }
          else if(instr.mnemonic=="slt") {
              // gia slt, kanoume unsigned sygkrisi
              unsigned int z1 = static_cast<unsigned int>(reg[instr.rs]);
              unsigned int z2 = static_cast<unsigned int>(reg[instr.rt]);
              aluResult = (z1 < z2) ? 1 : 0;
              reg[instr.rd] = aluResult;

              pc += 4;

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch

          }
          else if(instr.mnemonic=="sltu") {
              unsigned int u1 = static_cast<unsigned int>(reg[instr.rs]);
              unsigned int u2 = static_cast<unsigned int>(reg[instr.rt]);
              aluResult = (u1 < u2) ? 1 : 0;
              reg[instr.rd] = aluResult;

              pc += 4;

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //r type fetch

          }
          else if(instr.mnemonic=="slti") {
              aluResult = (reg[instr.rs] < instr.immediate) ? 1 : 0;
              reg[instr.rd] = aluResult;
              //i type and shift type

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch

              pc += 4;
          }
          else if(instr.mnemonic=="sltiu") {
              unsigned int u1 = static_cast<unsigned int>(reg[instr.rs]);
              unsigned int u2 = static_cast<unsigned int>(instr.immediate);
              aluResult = (u1 < u2) ? 1 : 0;
              reg[instr.rd] = aluResult;
              //i type and shift type

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch

              pc += 4;
          }
          else if(instr.mnemonic=="sll") {
              aluResult = reg[instr.rt] << instr.immediate;
              reg[instr.rd] = aluResult;
              //i type and shift type

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch


              if(instr.rd==32 && instr.rt==32 && instr.immediate==0)
              return false;

              pc += 4;

          }
          else if(instr.mnemonic=="srl") {
              aluResult = reg[instr.rt] >> instr.immediate;
              reg[instr.rd] = aluResult;
              //i type and shift type

              signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; //i type fetch

              pc += 4;
          }
          else if(instr.mnemonic=="lw") {
              int addr = reg[instr.rs] + instr.immediate;
              aluResult = addr;
              reg[instr.rd] = memory.read(addr);

              pc += 4;
              // lw

                signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; // lw fetch

          }
          else if(instr.mnemonic=="sw") {
              int addr = reg[instr.rt] + instr.immediate;
              aluResult = addr;
              memory.write(addr, reg[instr.rd]);
              pc += 4;

                signals={0,1,0,1,0,0,1,00,00,01,0,0,0}; // sw fetch


          }

          return true;
      }


      /*   stage 1:fetch*/

      vector<string> computeMonitorsFetch(const Instruction &instr, const ControlSignals &signals, int aluResult, int stage) {
        // Δημιουργούμε vector 34 θέσεων, όλες αρχικοποιημένες σε "-"
        vector<string> mon(34, "-");

        if (instr.type == InstrType::R_TYPE) {
            mon[0] = toHex(instr.PC+4);
            mon[1] = toHex(instr.PC);
            mon[3] = instr.fullLine;
            mon[4] = instr.mnemonic;
            mon[5] = getRegisterName(instr.rs);
            mon[6] = getRegisterName(instr.rt);
        }
        if (instr.type == InstrType::SHIFT_TYPE || instr.type == InstrType::I_TYPE) {
            mon[0] = toHex(instr.PC+4);
            mon[1] = toHex(instr.PC);
            mon[3] = instr.fullLine;
            mon[4] = instr.mnemonic;
            if(instr.type == InstrType::SHIFT_TYPE )
            mon[5] = getRegisterName(instr.rt);
            else
            mon[5] = getRegisterName(instr.rs);

            mon[6] = getRegisterName(instr.rd);
            mon[7] = toHex(instr.immediate);
        }
        else if (instr.type == InstrType::LUI_TYPE) {
            mon[0] = toHex(instr.PC+4);
            mon[1] = toHex(instr.PC);
            mon[3] = instr.fullLine;
            mon[4] = instr.mnemonic;
            mon[6] = getRegisterName(instr.rd);
            mon[7] = to_string(instr.immediate);

        }
        else if (instr.type == InstrType::MEM_TYPE) {
            mon[0] = toHex(instr.PC+4);
            mon[1] = toHex(instr.PC);
            mon[3] = instr.fullLine;
            mon[4] = instr.mnemonic;

            if(instr.mnemonic=="sw")
                mon[5] = getRegisterName(instr.rt);
            else
                mon[5] =getRegisterName(instr.rs);

            mon[6] = getRegisterName(instr.rd);
            if (instr.mnemonic == "sw")
            mon[7] = toHex(instr.immediate);
            else // lw
                mon[7] = toHex(cpu.registers[instr.rt]);
        }
        else if (instr.type == InstrType::BRANCH_TYPE) {
            int br = 0;
            if (cpu.registers[instr.rs] == cpu.registers[instr.rt])
                br = getLabelAddress(instr.labelTarget);
            br -= instr.PC;
            mon[0] = toHex(instr.PC);
            mon[1] = toHex(instr.PC);
            mon[3] = instr.fullLine;
            mon[4] = instr.mnemonic;
            mon[5] = getRegisterName(instr.rt);
            mon[6] = getRegisterName(instr.rs);
        }
        else if (instr.type == InstrType::J_TYPE) {
            mon[0] = toHex(instr.PC);
            mon[1] = toHex(instr.PC);
            mon[3] = instr.fullLine;
            mon[4] = instr.mnemonic;
            // Για jr, jal, j οι υπόλοιπες θέσεις παραμένουν "-"
        }

          mon[21] = to_string(signals.PCWriteCond);
          mon[22] = to_string(signals.PCWrite);
          mon[23] = to_string(signals.IorD);
          mon[24] = to_string(signals.MemRead);
          mon[25] = to_string(signals.MemWrite);
          mon[26] = to_string(signals.MemtoReg);
          mon[27] = to_string(signals.IRWrite);
          mon[28] = to_string(signals.PCSource);
          mon[29] = to_string(signals.AluOp);
          mon[30] = to_string(signals.ALUSrcB);
          mon[31] = to_string(signals.AluSrcA);
          mon[32] = to_string(signals.RegWrite);
          mon[33] = to_string(signals.RegDst);


        return mon;
    }

    //stage 2:decode
            vector<string> computeMonitorsDecode(const Instruction &instr, const ControlSignals &signals, int aluResult, int stage) {
                vector<string> mon(34, "-");

                if (instr.type == InstrType::R_TYPE) {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    mon[5] = getRegisterName(instr.rs);
                    mon[6] = getRegisterName(instr.rt);
                    mon[9] = getRegisterName(instr.rs);
                    mon[10] = getRegisterName(instr.rt);
                    mon[13] = toHex(cpu.registers[instr.rs]);
                    mon[14] = toHex(cpu.registers[instr.rt]);
                    mon[15] = toHex(cpu.registers[instr.rs]);
                    mon[16] = toHex(cpu.registers[instr.rt]);

                   //control unit

                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "0";
                    mon[27] = "0";
                    mon[28] = "0";
                    mon[29] = "0";
                    mon[30] = "11";
                    mon[31] = "0";
                    mon[32] = "0";
                    mon[33] = "0";
                }
                else if (instr.type == InstrType::SHIFT_TYPE || instr.type == InstrType::I_TYPE) {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    if(instr.type == InstrType::SHIFT_TYPE )
                        mon[5] = getRegisterName(instr.rt);
                    else//I-type
                        mon[5] = getRegisterName(instr.rs);
                    mon[6] = getRegisterName(instr.rd);
                    mon[7] = toHex(instr.immediate);
                    if(instr.type == InstrType::SHIFT_TYPE )
                        mon[9] = getRegisterName(instr.rt);
                    else //I-type
                        mon[9] = getRegisterName(instr.rs);
                    if(instr.type == InstrType::SHIFT_TYPE ){
                    mon[13]=toHex(cpu.registers[instr.rt]);
                    mon[15]=toHex(cpu.registers[instr.rt]);
                    }
                    else
                    {
                    mon[13] = toHex(cpu.registers[instr.rs]);
                    mon[15] = toHex(cpu.registers[instr.rs]);}

                    //control unit

                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "0";
                    mon[27] = "0";
                    mon[28] = "0";
                    mon[29] = "0";
                    mon[30] = "11";
                    mon[31] = "0";
                    mon[32] = "0";
                    mon[33] = "0";
                }
                else if (instr.type == InstrType::LUI_TYPE) {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    mon[6] = getRegisterName(instr.rd);
                    mon[7] = to_string(instr.immediate);




                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "0";
                    mon[27] = "0";
                    mon[28] = "0";
                    mon[29] = "0";
                    mon[30] = "11";
                    mon[31] = "0";
                    mon[32] = "0";
                    mon[33] = "0";
                }
                else if (instr.type == InstrType::MEM_TYPE) {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    if(instr.mnemonic=="sw")
                        mon[5] = getRegisterName(instr.rt);
                    else
                        mon[5] =getRegisterName(instr.rs);

                    mon[6] = getRegisterName(instr.rd);
                    if (instr.mnemonic == "sw") {
                        mon[7] = toHex(instr.immediate);
                        mon[9] = getRegisterName(instr.rt);
                        mon[10] = getRegisterName(instr.rd);
                        mon[13] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[14] = toHex(cpu.registers[instr.rd]);
                        mon[15] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[16] = toHex(cpu.registers[instr.rd]);


                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "0";
                        mon[30] = "11";
                        mon[31] = "0";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                    else { // lw
                        mon[7] = toHex(cpu.registers[instr.rt]);
                        mon[9] = getRegisterName(instr.rs);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[15] = toHex(cpu.registers[instr.rs]);



                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "0";
                        mon[30] = "11";
                        mon[31] = "0";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                }
                else if (instr.type == InstrType::BRANCH_TYPE) {
                    int br = 0, w = 0;
                    while (w < labels.size()) {
                        if (instr.labelTarget == labels[w].name) {
                            br = labels[w].address;
                            break;  // Εξέρχεσαι από το loop μόλις βρεθεί το label.
                        }
                        w++;
                    }
                    br -= instr.PC;


                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    mon[5] = getRegisterName(instr.rt);
                    mon[6] = getRegisterName(instr.rs);
                    mon[9] = getRegisterName(instr.rt);
                    mon[10] = getRegisterName(instr.rs);
                    mon[13] = toHex(cpu.registers[instr.rt]);
                    mon[14] = toHex(cpu.registers[instr.rs]);
                    mon[15] = toHex(cpu.registers[instr.rt]);
                    mon[16] = toHex(cpu.registers[instr.rs]);
                    mon[17] = toHex(instr.PC);
                    mon[18] = toHex(br);
                    mon[19] = instr.labelTarget;
                    mon[20] = instr.labelTarget;


                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "0";
                    mon[27] = "0";
                    mon[28] = "0";
                    mon[29] = "0";
                    mon[30] = "11";
                    mon[31] = "0";
                    mon[32] = "0";
                    mon[33] = "0";
                }
                else if (instr.type == InstrType::J_TYPE) {
                    if (instr.mnemonic == "jr") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = "$ra";
                        mon[6] = "$zero";
                        mon[9] = "$ra";
                        mon[10] = "$zero";
                        mon[13] = toHex(cpu.registers[31]);
                        mon[14] = "0";
                        mon[15] = toHex(cpu.registers[31]);
                        mon[16] = "0";



                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "10";
                        mon[30] = "00";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                    else if (instr.mnemonic == "jal") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;



                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "0";
                        mon[30] = "11";
                        mon[31] = "0";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                    else if (instr.mnemonic == "j") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;


                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "0";
                        mon[30] = "11";
                        mon[31] = "0";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                }




                return mon;
            }

            //stage 3:execute

            vector<string> computeMonitorsExecute(const Instruction &instr, const ControlSignals &signals, int aluResult, int stage) {
                vector<string> mon(34, "-");

                if (instr.type == InstrType::R_TYPE) {
                    // Διαχωρίζουμε την περίπτωση slt/sltu από τις άλλες εντολές R-type
                    if (instr.mnemonic == "slt" || instr.mnemonic == "sltu") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rs);
                        mon[6] = getRegisterName(instr.rt);
                        mon[9] = getRegisterName(instr.rs);
                        mon[10] = getRegisterName(instr.rt);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[14] = toHex(cpu.registers[instr.rt]);
                        mon[15] = toHex(cpu.registers[instr.rs]);
                        mon[16] = toHex(cpu.registers[instr.rt]);
                        mon[17] = toHex(cpu.registers[instr.rs]);
                        mon[18] = toHex(cpu.registers[instr.rt]);
                        mon[19] = toHex(cpu.registers[instr.rd]);
                        mon[20] = toHex(cpu.registers[instr.rd]);
                    }
                    else {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rs);
                        mon[6] = getRegisterName(instr.rt);
                        mon[9] = getRegisterName(instr.rs);
                        mon[10] = getRegisterName(instr.rt);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[14] = toHex(cpu.registers[instr.rt]);
                        mon[15] = toHex(cpu.registers[instr.rs]);
                        mon[16] = toHex(cpu.registers[instr.rt]);
                        mon[17] = toHex(cpu.registers[instr.rs]);
                        mon[18] = toHex(cpu.registers[instr.rt]);
                        mon[19] = toHex(cpu.registers[instr.rd]);
                        mon[20] = toHex(cpu.registers[instr.rd]);

                    }

                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "10";
                        mon[30] = "00";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";

                }
                else if (instr.type == InstrType::SHIFT_TYPE || instr.type == InstrType::I_TYPE) {
                    if (instr.mnemonic == "slti" || instr.mnemonic == "sltiu") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rs);
                        mon[6] = getRegisterName(instr.rd);
                        mon[7] = toHex(instr.immediate);
                        mon[9] = getRegisterName(instr.rs);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[15] = toHex(cpu.registers[instr.rs]);
                        mon[17] = toHex(cpu.registers[instr.rs]);
                        mon[18] = toHex(instr.immediate);
                        mon[19] = toHex(cpu.registers[instr.rd]);
                        mon[20] = toHex(cpu.registers[instr.rd]);
                    }
                    else {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;

                        if(instr.type == InstrType::SHIFT_TYPE )
                        mon[5] = getRegisterName(instr.rt);
                        else//I-type
                        mon[5] = getRegisterName(instr.rs);

                        mon[6] = getRegisterName(instr.rd);
                        mon[7] = toHex(instr.immediate);

                        if(instr.type == InstrType::SHIFT_TYPE )
                        mon[9] = getRegisterName(instr.rt);
                        else //I-type
                        mon[9] = getRegisterName(instr.rs);

                        if(instr.type == InstrType::SHIFT_TYPE )
                           {
                            mon[13]=toHex(cpu.registers[instr.rt]);
                            mon[15]=toHex(cpu.registers[instr.rt]);
                            }
                        else
                            {
                            mon[13] = toHex(cpu.registers[instr.rs]);
                            mon[15] = toHex(cpu.registers[instr.rs]);
                            }
                        if(instr.type == InstrType::SHIFT_TYPE )
                            {
                             mon[17]=toHex(cpu.registers[instr.rt]);
                             }
                         else
                             {
                             mon[17] = toHex(cpu.registers[instr.rs]);
                             }
                        mon[18] = toHex(instr.immediate);
                        mon[19] = toHex(cpu.registers[instr.rd]);
                        mon[20] = toHex(cpu.registers[instr.rd]);
                    }
                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "10";
                        mon[30] = "10";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                }
                else if (instr.type == InstrType::LUI_TYPE) {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    mon[6] = getRegisterName(instr.rd);
                    mon[7] = to_string(instr.immediate);
                    mon[18] = to_string(instr.immediate);
                    mon[19] = toHex(aluResult);
                    mon[20] = toHex(aluResult);

                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "10";
                        mon[30] = "10";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                }
                else if (instr.type == InstrType::MEM_TYPE) {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;

                    if(instr.mnemonic=="sw")
                         mon[5] = getRegisterName(instr.rt);
                    else//lw
                        mon[5] = getRegisterName(instr.rs);

                    mon[6] = getRegisterName(instr.rd);
                    if (instr.mnemonic == "sw") {
                        mon[7] = toHex(instr.immediate);
                        mon[9] = getRegisterName(instr.rt);
                        mon[10] = getRegisterName(instr.rd);
                        mon[13] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[14] = toHex(cpu.registers[instr.rd]);
                        mon[15] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[16] = toHex(cpu.registers[instr.rd]);
                        mon[17] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[18] = toHex(instr.immediate);
                        mon[19] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[20] = toHex(cpu.registers[instr.rt] + instr.immediate);
                    }
                    else { // lw
                        mon[7] = toHex(cpu.registers[instr.rt]);
                        mon[9] = getRegisterName(instr.rs);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[15] = toHex(cpu.registers[instr.rs]);
                        mon[17] = toHex(cpu.registers[instr.rs]);
                        mon[18] = toHex(instr.immediate);
                        mon[19] = toHex(cpu.registers[instr.rs]);
                        mon[20] = toHex(cpu.registers[instr.rs]);
                    }
                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "0";
                        mon[29] = "00";
                        mon[30] = "10";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                }
                else if (instr.type == InstrType::BRANCH_TYPE) {

                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    mon[5] = getRegisterName(instr.rt);
                    mon[6] = getRegisterName(instr.rs);
                    mon[9] = getRegisterName(instr.rt);
                    mon[10] = getRegisterName(instr.rs);
                    mon[13] = toHex(cpu.registers[instr.rt]);
                    mon[14] = toHex(cpu.registers[instr.rs]);
                    mon[15] = toHex(cpu.registers[instr.rt]);
                    mon[16] = toHex(cpu.registers[instr.rs]);
                    mon[17] = toHex(cpu.registers[instr.rt]);
                    mon[18] = toHex(cpu.registers[instr.rs]);
                    mon[19] = toHex(cpu.registers[instr.rs] - cpu.registers[instr.rt]);

                        mon[21] = "1";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "01";
                        mon[29] = "01";
                        mon[30] = "00";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                }
                else if (instr.type == InstrType::J_TYPE) {
                    if (instr.mnemonic == "jr") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = "$ra";
                        mon[6] = "$zero";
                        mon[9] = "$ra";
                        mon[10] = "$zero";
                        mon[13] = toHex(cpu.registers[31]);
                        mon[14] = "0";
                        mon[15] = toHex(cpu.registers[31]);
                        mon[16] = "0";
                        mon[17] = toHex(cpu.registers[31]);
                        mon[18] = "0";
                        mon[19] = toHex(cpu.registers[31]);
                        mon[20] = toHex(cpu.registers[31]);
                        // Οι υπόλοιπες θέσεις μένουν "-"
                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "00";
                        mon[29] = "10";
                        mon[30] = "00";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                    }

                    if(instr.mnemonic=="j")
                    {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] ="-";
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[20]= instr.labelTarget;
                        mon[21] = "0";
                        mon[22] = "1";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "10";
                        mon[29] = "00";
                        mon[30] = "11";
                        mon[31] = "0";
                        mon[32] = "1";
                        mon[33] = "0";
                    }
                     if(instr.mnemonic=="jal")
                     {
                        int target = getLabelAddress(instr.labelTarget);
                            if(target != -1) {
                                pc = target;
                            }

                            mon[0] = toHex(instr.PC+4);
                            mon[1] = toHex(instr.PC);
                            mon[2] ="-";
                            mon[3] = instr.fullLine;
                            mon[4] = instr.mnemonic;
                            mon[11]="$ra";
                            mon[12]=toHex(cpu.registers[31]);
                            mon[17]=toHex(instr.PC);
                            mon[18]="4";
                            mon[19]=toHex(cpu.registers[31]);
                            mon[20]=toHex(cpu.registers[31]);

                        mon[21] = "0";
                        mon[22] = "1";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "10";
                        mon[29] = "00";
                        mon[30] = "01";
                        mon[31] = "0";
                        mon[32] = "1";
                        mon[33] = "0";
                     }
                }

                return mon;
            }

            //stage 4: memory access
            vector<string> computeMonitorsMemory(const Instruction &instr, const ControlSignals &signals, int aluResult, int stage) {
                // Δημιουργούμε vector 34 θέσεων, ώστε να έχουμε δείκτες 0 έως 34
                vector<string> mon(34, "-");

                if (instr.type == InstrType::R_TYPE) {
                    if (instr.mnemonic == "slt" || instr.mnemonic == "sltu") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] = "-";
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rs);
                        mon[6] = getRegisterName(instr.rt);
                        mon[7] = "-";
                        mon[8] = "-";
                        mon[9] = getRegisterName(instr.rs);
                        mon[10] = getRegisterName(instr.rt);
                        mon[11] = getRegisterName(instr.rd);
                        mon[12] = toHex(cpu.registers[instr.rd]);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[14] = toHex(cpu.registers[instr.rt]);
                        mon[15] = toHex(cpu.registers[instr.rs]);
                        mon[16] = toHex(cpu.registers[instr.rt]);
                        mon[17] = toHex(cpu.registers[instr.rs]);
                        mon[18] = toHex(cpu.registers[instr.rt]);
                        mon[19] = toHex(cpu.registers[instr.rd]); // αποτέλεσμα για slt/sltu
                        mon[20] = toHex(cpu.registers[instr.rd]);


                    }
                    else {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] = "-";
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rs);
                        mon[6] = getRegisterName(instr.rt);
                        mon[7] = "-";
                        mon[8] = "-";
                        mon[9] = getRegisterName(instr.rs);
                        mon[10] = getRegisterName(instr.rt);
                        mon[11] = getRegisterName(instr.rd);
                        mon[12] = toHex(cpu.registers[instr.rd]);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[14] = toHex(cpu.registers[instr.rt]);
                        mon[15] = toHex(cpu.registers[instr.rs]);
                        mon[16] = toHex(cpu.registers[instr.rt]);
                        mon[17] = toHex(cpu.registers[instr.rs]);
                        mon[18] = toHex(cpu.registers[instr.rt]);
                        mon[19] = toHex(cpu.registers[instr.rd]);
                        mon[20] = toHex(cpu.registers[instr.rd]);
                    }
                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "0";
                    mon[27] = "0";
                    mon[28] = "00";
                    mon[29] = "10";
                    mon[30] = "00";
                    mon[31] = "1";
                    mon[32] = "1";
                    mon[33] = "1";
                }
                if (instr.type == InstrType::SHIFT_TYPE || instr.type == InstrType::I_TYPE) {
                    if (instr.mnemonic == "slti" || instr.mnemonic == "sltiu") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] = "-";
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rs);
                        mon[6] = getRegisterName(instr.rd);
                        mon[7] = toHex(instr.immediate);
                        mon[8] = "-";
                        mon[9] = getRegisterName(instr.rs);
                        mon[10] = "-";
                        mon[11] = getRegisterName(instr.rd);
                        mon[12] = toHex(cpu.registers[instr.rd]);
                        mon[13] = toHex(cpu.registers[instr.rs]);
                        mon[15] = toHex(cpu.registers[instr.rs]);
                        mon[17] = toHex(cpu.registers[instr.rs]);
                        mon[18] = toHex(cpu.registers[instr.rs]);
                        mon[19] = toHex(cpu.registers[instr.rd]);
                        mon[20] = toHex(cpu.registers[instr.rd]);
                    }
                    else {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] = "-";
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;

                        if(instr.type == InstrType::SHIFT_TYPE )
                            mon[5] = getRegisterName(instr.rt);
                        else//I-type
                            mon[5] = getRegisterName(instr.rs);

                        mon[6] = getRegisterName(instr.rd);
                        mon[7] = toHex(instr.immediate);

                        if(instr.type == InstrType::SHIFT_TYPE )
                            mon[9] = getRegisterName(instr.rt);
                        else //I-type
                            mon[9] = getRegisterName(instr.rs);

                        if(instr.type == InstrType::SHIFT_TYPE )
                            mon[11] = getRegisterName(instr.rt);
                        else//I-type
                            mon[11] = getRegisterName(instr.rd);

                        mon[12] = toHex(cpu.registers[instr.rd]);

                        if(instr.type == InstrType::SHIFT_TYPE ){
                            mon[13]=toHex(cpu.registers[instr.rt]);
                            mon[15]=toHex(cpu.registers[instr.rt]);
                            }
                        else
                            {
                            mon[13] = toHex(cpu.registers[instr.rs]);
                            mon[15] = toHex(cpu.registers[instr.rs]);}

                        if(instr.type == InstrType::SHIFT_TYPE )
                           {
                            mon[17]=toHex(cpu.registers[instr.rt]);

                            }
                        else
                            {
                            mon[17] = toHex(cpu.registers[instr.rs]);

                            }
                        mon[18] = toHex(instr.immediate);
                        mon[19] = toHex(cpu.registers[instr.rd]);
                        mon[20] = toHex(cpu.registers[instr.rd]);
                    }
                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "0";
                    mon[27] = "0";
                    mon[28] = "00";
                    mon[29] = "10";
                    mon[30] = "10";
                    mon[31] = "1";
                    mon[32] = "1";
                    mon[33] = "0";
                }
                 if (instr.type == InstrType::LUI_TYPE) {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);

                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;

                    mon[6] = getRegisterName(instr.rd);
                    mon[7] = to_string(instr.immediate);

                    mon[11] = getRegisterName(instr.rd);
                    mon[12] = toHex(cpu.registers[instr.rd]);

                    mon[18] = to_string(instr.immediate);
                    mon[19] = toHex(cpu.registers[instr.rd]);
                    mon[20] = toHex(cpu.registers[instr.rd]);
                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "0";
                    mon[27] = "0";
                    mon[28] = "00";
                    mon[29] = "10";
                    mon[30] = "10";
                    mon[31] = "1";
                    mon[32] = "1";
                    mon[33] = "0";
                }
                if (instr.type == InstrType::MEM_TYPE) {
                    if (instr.mnemonic == "sw") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] = toHex(cpu.registers[instr.rd]);
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rt);
                        mon[6] = getRegisterName(instr.rd);
                        mon[7] =  toHex(instr.immediate);
                        mon[8] = "-";
                        mon[9] = getRegisterName(instr.rt);
                        mon[10] = getRegisterName(instr.rd);
                        mon[11] = "-";
                        mon[12] = "-";
                        mon[13] = toHex(cpu.registers[instr.rt]); // τιμή για store word
                        mon[14] = toHex(cpu.registers[instr.rd]);
                        mon[15] = toHex(cpu.registers[instr.rt]); // τιμή για store word
                        mon[16] = toHex(cpu.registers[instr.rd]);
                        mon[17] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[18] = toHex(instr.immediate);
                        mon[19] = toHex(cpu.registers[instr.rt] + instr.immediate);
                        mon[20] = toHex(cpu.registers[instr.rt] + instr.immediate);

                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "1";
                        mon[24] = "0";
                        mon[25] = "1";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "00";
                        mon[29] = "00";
                        mon[30] = "10";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                    else { // lw
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] = "-";
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = getRegisterName(instr.rs);
                        mon[6] = getRegisterName(instr.rd);
                        mon[7] = toHex(cpu.registers[instr.rt]);
                        mon[8] = toHex(cpu.registers[instr.rd]);
                        mon[9] = getRegisterName(instr.rs);

                        mon[13] = toHex(cpu.registers[instr.rs]); // τιμή για load word
                        mon[14] = "-";
                        mon[15] = toHex(cpu.registers[instr.rs]); // τιμή για load word
                        mon[16] = "-";
                        mon[17] = toHex(cpu.registers[instr.rs]); // τιμή για load word
                        mon[18] = toHex(cpu.registers[instr.rt]);
                        mon[19] = toHex(cpu.registers[instr.rs]);
                        mon[20] = toHex(cpu.registers[instr.rs]);
                        mon[21] = "0";
                        mon[22] = "0";
                        mon[23] = "1";
                        mon[24] = "1";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "00";
                        mon[29] = "00";
                        mon[30] = "10";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                }
                 if (instr.type == InstrType::J_TYPE) {
                    if (instr.mnemonic == "jr") {
                        mon[0] = toHex(instr.PC+4);
                        mon[1] = toHex(instr.PC);
                        mon[2] = "-";
                        mon[3] = instr.fullLine;
                        mon[4] = instr.mnemonic;
                        mon[5] = "$ra";
                        mon[6] = "$zero";
                        mon[7] = "-";
                        mon[8] = "-";
                        mon[9] = "$ra";
                        mon[10] = "$zero";
                        mon[11] = "-";
                        mon[12] = "-";
                        mon[13] = toHex(cpu.registers[31]);
                        mon[14] = "0";
                        mon[15] = toHex(cpu.registers[31]);
                        mon[16] = "0";
                        mon[17] = toHex(cpu.registers[31]);
                        mon[18] = "0";
                        mon[19] = toHex(cpu.registers[31]);
                        mon[20] = toHex(cpu.registers[31]);

                        mon[21] = "0";
                        mon[22] = "1";
                        mon[23] = "0";
                        mon[24] = "0";
                        mon[25] = "0";
                        mon[26] = "0";
                        mon[27] = "0";
                        mon[28] = "01";
                        mon[29] = "10";
                        mon[30] = "00";
                        mon[31] = "1";
                        mon[32] = "0";
                        mon[33] = "0";
                    }
                }

                return mon;
            }


       //stage 5 write back
            vector<string> computeMonitorsWriteBack(const Instruction &instr, const ControlSignals &signals, int aluResult, int stage) {
                vector<string> mon(34, "-");

                if (instr.mnemonic == "lw") {
                    mon[0] = toHex(instr.PC+4);
                    mon[1] = toHex(instr.PC);
                    mon[2] = "-";
                    mon[3] = instr.fullLine;
                    mon[4] = instr.mnemonic;
                    mon[5] = getRegisterName(instr.rs);
                    mon[6] = getRegisterName(instr.rd);
                    mon[7] = toHex(cpu.registers[instr.rt]);
                    mon[8] = toHex(cpu.registers[instr.rd]);
                    mon[9] = getRegisterName(instr.rs);
                    mon[11] = getRegisterName(instr.rd);
                    mon[12] = toHex(cpu.registers[instr.rd]);
                    mon[13] = toHex(cpu.registers[instr.rs]);
                    mon[15] = toHex(cpu.registers[instr.rs]);
                    mon[17] = toHex(cpu.registers[instr.rs]);
                    mon[18] = toHex(cpu.registers[instr.rt]);
                    mon[19] = toHex(cpu.registers[instr.rs]);
                    mon[20] = toHex(cpu.registers[instr.rs]);
                    mon[21] = "0";
                    mon[22] = "0";
                    mon[23] = "0";
                    mon[24] = "0";
                    mon[25] = "0";
                    mon[26] = "1";
                    mon[27] = "0";
                    mon[28] = "00";
                    mon[29] = "00";
                    mon[30] = "10";
                    mon[31] = "1";
                    mon[32] = "1";
                    mon[33] = "0";
                }

                return mon;
            }
  // --------------------------
      // computeMonitors: epistrefei ena vector 34 strings me ta pedia gia to output
      // --------------------------
            vector<string> computeMonitors(const Instruction &instr, const ControlSignals &signals, int aluResult, int stage) {
                vector<string> monitorss(34);

                if (stage == 1) {
                    monitorss = computeMonitorsFetch(instr,signals, aluResult,stage);
                }
                if (stage == 2) {
                    monitorss = computeMonitorsDecode(instr,signals, aluResult,stage);
                }
                if (stage == 3) {
                    monitorss = computeMonitorsExecute(instr,signals, aluResult,stage);
                }
                 if (stage == 4) {
                    monitorss = computeMonitorsMemory(instr,signals, aluResult,stage);
                }
                 if (stage == 5) {
                    monitorss = computeMonitorsWriteBack(instr,signals, aluResult,stage);
                }






          return monitorss;
      }



      // --------------------------
      // printCycleState: ektypwnei tin katastasi tou kuklou sto arxeio eksodou
      // --------------------------
      void printCycleState(int cycle, const Instruction &instr, const ControlSignals &signals, int aluResult, ofstream &out,int &stage) {

          if((stage==1 || stage==2 || stage==3) &&  ((instr.type == InstrType::R_TYPE)|| (instr.type == InstrType::I_TYPE) || (instr.mnemonic=="jr") || (instr.type == InstrType::LUI_TYPE)|| instr.mnemonic=="sw" || (instr.type==InstrType::SHIFT_TYPE)))
          {
            out << "-----Cycle " << cycle << "-----\n";
            out << "Registers:\n";

            out << toHex(instr.PC+4) << "\t";
           for (int i = 0; i < 32; i++)
             {
            out << toHex(cpu.registerslast[i]) << "\t";
             }
             out << "\n\n";
          out << "Monitors:\n";
          vector<string> monout = computeMonitors(instr, signals, aluResult,stage);
          for (size_t i = 0; i < monout.size(); i++) {
            // If the index is from 21 to 33, print in binary.
            if (i >= 21 && i <= 33) {
                int value = stoi(monout[i]);
                // For indices 28, 29, and 30, force a 2-bit output using bitset<2>
                if (i == 28 || i == 29 || i == 30) {
                    out << std::bitset<2>(value) << "\t";
                } else {
                    // Otherwise, use the helper function toBinary (or you can decide on a fixed width)
                    out << toBinary(value) << "\t";
                }
            } else {
                // For other indices, print as is.
                out << monout[i] << "\t";
            }
        }

          out << "\n\n";

          if(instr.mnemonic=="sw" && stage>=4)
          out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
          //an ektelite i sw kai ine prin to stadio 4:execute tote tiponete ena ligotero memory state
          else if(instr.mnemonic=="sw" && stage<4)
          out << "Memory State: \n" << memory.getMemoryStateSW() << "\n\n";
          else
           out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
          }

         if(stage==4 && ((instr.type == InstrType::R_TYPE)|| (instr.type == InstrType::I_TYPE) || (instr.mnemonic=="jr")|| (instr.type == InstrType::LUI_TYPE) || (instr.mnemonic=="sw") || (instr.type==InstrType::SHIFT_TYPE)))
          {
            out << "-----Cycle " << cycle << "-----\n";
            out << "Registers:\n";

            out << toHex(instr.PC+4) << "\t";
            for (int p = 0; p < 32; p++)
            {
             out << toHex(cpu.registers[p]) << "\t";
            }
            out << "\n\n";
          out << "Monitors:\n";
          vector<string> monout = computeMonitors(instr, signals, aluResult,stage);
          for (size_t i = 0; i < monout.size(); i++) {
            // If the index is from 21 to 33, print in binary.
            if (i >= 21 && i <= 33) {
                int value = stoi(monout[i]);
                // For indices 28, 29, and 30, force a 2-bit output using bitset<2>
                if (i == 28 || i == 29 || i == 30) {
                    out << std::bitset<2>(value) << "\t";
                } else {
                    // Otherwise, use the helper function toBinary (or you can decide on a fixed width)
                    out << toBinary(value) << "\t";
                }
            } else {
                // For other indices, print as is.
                out << monout[i] << "\t";
            }
        }


          out << "\n\n";

          if(instr.mnemonic=="sw" && stage>=4)
          out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
          //an ektelite i sw kai ine prin to stadio 4:execute tote tiponete ena ligotero memory state
          else if(instr.mnemonic=="sw" && stage<4)
          out << "Memory State: \n" << memory.getMemoryStateSW() << "\n\n";
          else
           out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
          }

         if((((instr.type == InstrType::J_TYPE) && instr.mnemonic !="jr" )|| (instr.type == InstrType::BRANCH_TYPE)) && (stage==1 || stage==2))
         {
            out << "-----Cycle " << cycle << "-----\n";
            out << "Registers:\n";

            out << toHex(instr.PC+4) << "\t";
            for (int t = 0; t< 32; t++)
            {
           out << toHex(cpu.registerslast[t]) << "\t";
            }
            out << "\n\n";
          out << "Monitors:\n";
          vector<string> monout = computeMonitors(instr, signals, aluResult,stage);
          for (size_t i = 0; i < monout.size(); i++) {
            // If the index is from 21 to 33, print in binary.
            if (i >= 21 && i <= 33) {
                int value = stoi(monout[i]);
                // For indices 28, 29, and 30, force a 2-bit output using bitset<2>
                if (i == 28 || i == 29 || i == 30) {
                    out << std::bitset<2>(value) << "\t";
                } else {
                    // Otherwise, use the helper function toBinary (or you can decide on a fixed width)
                    out << toBinary(value) << "\t";
                }
            } else {
                // For other indices, print as is.
                out << monout[i] << "\t";
            }
        }

          out << "\n\n";

          if(instr.mnemonic=="sw" && stage>=4)
          out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
          //an ektelite i sw kai ine prin to stadio 4:execute tote tiponete ena ligotero memory state
          else if(instr.mnemonic=="sw" && stage<4)
          out << "Memory State: \n" << memory.getMemoryStateSW() << "\n\n";
          else
           out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
         }

         if(stage==3 &&(((instr.type == InstrType::J_TYPE) && instr.mnemonic !="jr" ) || (instr.type == InstrType::BRANCH_TYPE) ) )
           {
            out << "-----Cycle " << cycle << "-----\n";
            out << "Registers:\n";

            out << toHex(instr.PC+4) << "\t";
            for (int t = 0; t< 32; t++)
            {
           out << toHex(cpu.registers[t]) << "\t";
            }
            out << "\n\n";
          out << "Monitors : \n";
          vector<string> monout = computeMonitors(instr, signals, aluResult,stage);
          for (size_t i = 0; i < monout.size(); i++) {
            // If the index is from 21 to 33, print in binary.
            if (i >= 21 && i <= 33) {
                int value = stoi(monout[i]);
                // For indices 28, 29, and 30, force a 2-bit output using bitset<2>
                if (i == 28 || i == 29 || i == 30) {
                    out << std::bitset<2>(value) << "\t";
                } else {
                    // Otherwise, use the helper function toBinary (or you can decide on a fixed width)
                    out << toBinary(value) << "\t";
                }
            } else {
                // For other indices, print as is.
                out << monout[i] << "\t";
            }
        }

          out << "\n\n";

          if(instr.mnemonic=="sw" && stage>=4)
          out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
          //an ektelite i sw kai ine prin to stadio 4:execute tote tiponete ena ligotero memory state
          else if(instr.mnemonic=="sw" && stage<4)
          out << "Memory State: \n" << memory.getMemoryStateSW() << "\n\n";
          else
           out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
           }

           if((stage==1 || stage==2 || stage==3 ||stage==4) && instr.mnemonic=="lw")
           {
            out << "-----Cycle " << cycle << "-----\n";
            out << "Registers:\n";

            out << toHex(instr.PC+4) << "\t";
            for (int i = 0; i < 32; i++)
             {
            out << toHex(cpu.registerslast[i]) << "\t";
             }
             out << "\n\n";
          out << "Monitors:\n";
          vector<string> monout = computeMonitors(instr, signals, aluResult,stage);
          for (size_t i = 0; i < monout.size(); i++) {
            // If the index is from 21 to 33, print in binary.
            if (i >= 21 && i <= 33) {
                int value = stoi(monout[i]);
                // For indices 28, 29, and 30, force a 2-bit output using bitset<2>
                if (i == 28 || i == 29 || i == 30) {
                    out << std::bitset<2>(value) << "\t";
                } else {
                    // Otherwise, use the helper function toBinary (or you can decide on a fixed width)
                    out << toBinary(value) << "\t";
                }
            } else {
                // For other indices, print as is.
                out << monout[i] << "\t";
            }
        }

          out << "\n\n";

          if(instr.mnemonic=="sw" && stage>=4)
          out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
          //an ektelite i sw kai ine prin to stadio 4:execute tote tiponete ena ligotero memory state
          else if(instr.mnemonic=="sw" && stage<4)
          out << "Memory State: \n" << memory.getMemoryStateSW() << "\n\n";
          else
           out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
           }

           if(stage==5 && instr.mnemonic=="lw")
           {
            out << "-----Cycle " << cycle << "-----\n";
            out << "Registers: \n";

            out << toHex(instr.PC+4) << "\t";
            for (int i = 0; i < 32; i++)
             {
            out << toHex(cpu.registers[i]) << "\t";
             }
             out << "\n\n";
          out << "Monitors:\n";
          vector<string> monout = computeMonitors(instr, signals, aluResult,stage);
          for (size_t i = 0; i < monout.size(); i++) {
            // If the index is from 21 to 33, print in binary.
            if (i >= 21 && i <= 33) {
                int value = stoi(monout[i]);
                // For indices 28, 29, and 30, force a 2-bit output using bitset<2>
                if (i == 28 || i == 29 || i == 30) {
                    out << std::bitset<2>(value) << "\t";
                } else {
                    // Otherwise, use the helper function toBinary (or you can decide on a fixed width)
                    out << toBinary(value) << "\t";
                }
            } else {
                // For other indices, print as is.
                out << monout[i] << "\t";
            }
        }

            out << "\n\n";

            if(instr.mnemonic=="sw" && stage>=4)
            out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
            //an ektelite i sw kai ine prin to stadio 4:execute tote tiponete ena ligotero memory state
            else if(instr.mnemonic=="sw" && stage<4)
            out << "Memory State: \n" << memory.getMemoryStateSW() << "\n\n";
            else
             out << "Memory State: \n" << memory.getMemoryState() << "\n\n";
           }

      }

      // printFinalState: ektypwnei to teliko state sto arxeio eksodou
      void printFinalState(ofstream &out) {
          out << "-----Final State-----\n";
          out << "Registers:\n";
          out << toHex(pc+4) << "\t";
          for (int i = 0; i < 32; i++) {
              out << toHex(cpu.registers[i]) << "\t";
          }
          out << "\n\nMemory State: \n" << memory.getMemoryState() << "\n\n";
          out << "Total Cycles: \n" << (cycleCount - 1)<<endl;
          out << endl;
          out << "User Time:"<<endl;

      }

  public:
      Simulator() : cycleCount(1), pc(0) { }

      // loadInstructions: diavazei ta entoles apo to input arxeio kai ta apothikeuei stin lista
      void loadInstructions(const string &filename) {
          ifstream fin(filename);
          if(!fin) {
              cerr << "Den einai dynati i anagnosi tou arxeiou: " << filename << endl;
              exit(1);
          }
          string line;
          int currentPC = 0;
          while(getline(fin, line)) {
              int before = instructions.size();
              parseLine(line, currentPC);
              int after = instructions.size();
              if(after > before)
                  currentPC += 4;  // auksanei to PC kata 4 gia kathe entoli
          }
          fin.close();
      }

      // run: trexei tin prosomiosi, ektypwnei ta kukla pou epilegoume kai to teliko state sto output file
      void run(const vector<int> &printCycles, const string &outputFile,
               const string &studentName, const string &studentID,int &stage,auto &start) {
          ofstream fout(outputFile);
          bool cont=1;
          if(!fout) {
              cerr << "Den einai dynati i dimiourgia tou arxeiou eksodou: " << outputFile << endl;
              exit(1);
          }
          fout << "Name: " << studentName << "\n";
          fout << "University ID: " << studentID << "\n\n";

          ControlSignals signals;
          int aluResult = 0;

          while(true)
           {
              int cyclesperinst=0; //for multicycle
              int index = pc / 4;
              if(index >= instructions.size()) {
                break; // Τερματισμός προσομοίωσης, δεν υπάρχουν άλλες εντολές
            }

              Instruction currentInstr = instructions[index];

              // Set cycle count based on instruction type/mnemonic:
              cyclesperinst = getCycleCountForInstruction(currentInstr);

              cont = executeInstruction(currentInstr, signals, aluResult,stage);

              if(!cont)
              {
                cycleCount+=4;
                break;
              }//an vrike sll zero zero,0 prostheti tous apaitoumenous kiklous gia ektelesi alla den tipononte

              for(stage = 1; stage <= cyclesperinst; stage++)
              {
                if(find(printCycles.begin(), printCycles.end(), cycleCount) != printCycles.end())
                    printCycleState(cycleCount, currentInstr, signals, aluResult, fout, stage);

                cycleCount++;
             }


             }
          printFinalState(fout);
        //stamato xronometro
          auto end = high_resolution_clock::now();
        // ipologismos xronou se ns
          auto duration = duration_cast<nanoseconds>(end - start);
          fout<<duration.count()<<"ns"<<endl;
          fout<<endl;
          fout.close();
      }

    };

  // --------------------------
  // main: diavazei tis parametrous kai trexei tin simulation
  // --------------------------
  int main(){
      int totalprintcycles;//plithos kiklon gia isodo
      int stage=0;// 1:fetch 2:decode 3:execute 4:memory access 5:write back
      cout << "Enter the number of cycles you want to print: ";
      cin >> totalprintcycles;

      vector<int> printCycles(totalprintcycles);
      cout << "Enter the cycle numbers (separated by spaces): ";
      for (int i = 0; i < totalprintcycles; i++){
          cin >> printCycles[i];
      }

      //arxizo xronometro
      auto start = high_resolution_clock::now();

      string studentName = "Iosif Nicolaou";
      string studentID = "UC10xxxxx";

      Simulator sim;
      // Fortwnei ta entoles apo to input arxeio
      sim.loadInstructions("mat_mul_lab2_2025.txt");
      // Trexei tin simulation kai ektypwnei tous kiklous pou epilegoume sto output file
      sim.run(printCycles, "UC10xxxxx_mat_mul_lab2_Iosif_Nicolaou.txt", studentName, studentID,stage,start);

      //stamato xronometro
      auto end = high_resolution_clock::now();

      cout << "Simulation complete.\n";

      return 0;
  }
