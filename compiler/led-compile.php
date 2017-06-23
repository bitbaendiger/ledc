<?PHP

  /**
   * LED Assembler
   * -------------
   * Simple compiler for LED-Assembler.
   * 
   *  COLOR     - Set the color-register
   *  SETDEST   - Set the destination-register to address:
   *               0-15: Index of a physically attached LED-Strip
   *              16-31: Index of a virtual LED-Strip
   *              32-64: Index of a Strip-Group (See GROUPADD)
   *  SETS      - Set a series of $Length LEDs beginning at $Start
   *              on current Destination to current color
   *  SETI      - Set a number (max 6) of indexed LEDs on current
   *              Destination to current color
   *  COPY      - Copy a set of LED-Pixel
   *  MIRRORS   - Copy an entire LED-Strip to a series of others
   *  SHIFTR    - Move an entire set of LED-Settings to right
   *  GROUPADDS - Add a set of LED-Strips to a group
   *  GROUPADDI - Add some indexed LED-Strips to a group
   *  FRAME     - Output current Settings on LED-Strips and wait
   *  GOTO      - Move the programm-counter somewhere
   * 
   * Binary representation (32-Bit Words, MSB):
   * 
   *   Instruction		Parameters					Size
   *  -------------------------------------------------------------------------------
   *   111  COLOR		bright[5], red[8], green[8], blue[8]		 32
   *   0000 SETDEST		dest[6]						 10
   *   0001 SETS		start[4], length[4]				 12
   *   0010 SETI		cnt[4], i1[4], i2[4], i3[4], i4[4], i5[4], i6[4] 32
   *   0011 
   *   0100 
   *   0101 COPY		src[6], dst[6], start[4], length[4], offset[4]	 28
   *   0110 MIRRORS		src[6], dst[6], count[6]			 22
   *   0111 
   *   1000 SHIFTR		cnt[2], strt1[6], len1[4], strt2[6], len2[4]	 26
   *   1001 
   *   1010 GROUPADDS		grp[6], start[6], length[6]			 22
   *   1011 GROUPADDI		grp[6], cnt[2], dst1[6], dst2[6], dst3[6]	 30
   *   1100 FRAME		wait[16]					 20
   *   1101 GOTO		offset[16]					 20
   * 
   * The COLOR-Instruction is a special case and the instruction-code is only 3-bit
   * wide as the parameters require more space to fit into the single command.
   **/
  
  define ('OP_COPY',    0x05);
  define ('OP_MIRRORS', 0x06);
  define ('OP_SHIFTR',  0x08);
  
  function compile_error ($Error, $Code, $Pos = null) {
    global $lineNumber;
    
    echo
      'Programm could not be compiled.', "\n",
      'Error on line ', $lineNumber, ': ', $Error, "\n";
    
    exit (1);
  }
  
  $fIn = fopen ($argv [1], 'r');
  $Opcode = '';
  $Offsets = array ();
  $Defines = array ('default_wait' => 250);
  $lineNumber = 0;
  
  while ($Line = fgets ($fIn)) {
    // Parse the line
    $lineNumber++;
    $Line = trim (str_replace ("\t", ' ', $Line));
    
    if ((strlen ($Line) == 0) || ($Line [0] == '#'))
      continue;
    
    if (($p1 = strpos ($Line, ' ')) !== false)
      if ((($p2 = strpos ($Line, ',')) === false) || ($p1 < $p2))
        $Line [$p1] = ',';
    
    $Words = explode (',', $Line);
    unset ($Line);
    
    // Pre-process all words
    foreach ($Words as $k=>$l)
      // Make sure Op-Word is upper case
      if ($k == 0)
        $Words [0] = (strpos ($l, ':') === false ? strtoupper (trim ($l)) : trim ($l));
      elseif (($k != 1) || !in_array ($Words [0], array ('GOTO', 'DEFINE'))) {
        // Clean up white-spaces
        $l = trim ($l);
        
        // Check for a variable
        if ($l [0] == '$') {
          $l = substr ($l, 1);
          
          if (!isset ($Defines [$l])) {
            trigger_error ('Unknown reference to ' . $l);
            $l = 0;
          } else
            $l = $Defines [$l];
        }
        
        // Store as integer
        $Words [$k] = (int)$l;
      } else
        $Words [$k] = trim ($l);
    
    // Process
    $Word = 0;
    
    switch ($Words [0]) {
      // Set COLOR-Register
      case 'COLOR':
        if (count ($Words) != 5)
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 0) || ($Words [1] > 31))
          compile_error ('Invalid Brightness', $Words, 1);
        
        if (($Words [2] < 0) || ($Words [2] > 255))
          compile_error ('Invalid Red-Value', $Words, 2);
        
        if (($Words [3] < 0) || ($Words [3] > 255))
          compile_error ('Invalid Red-Value', $Words, 3);
        
        if (($Words [4] < 0) || ($Words [4] > 255))
          compile_error ('Invalid Red-Value', $Words, 4);
        
        $Word =
          (7 << 29) |                   // Actual instruction
          (($Words [1] & 0x1F) << 24) | // Brightness
          (($Words [2] & 0xFF) << 16) | // Red
          (($Words [3] & 0xFF) <<  8) | // Green
          (($Words [4] & 0xFF));        // Blue
        
        break;
      // Set DESTINATION-Register
      case 'SETDEST':
        if (count ($Words) != 2)
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 0) || ($Words [1] > 63))
          compile_error ('Invalid destination', $Words, 1);
        
        $Word =
          (0 << 28) |                  // Actual instruction
          (($Words [1] & 0x3F) << 22); // Destination-Address
        
        break;
      // Set a series of LED-pixel on current destination to current color
      case 'SETS':
        if (count ($Words) != 3)
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 0) || ($Words [1] > 15))
          compile_error ('Invalid Start-Offset', $Words, 1);
        
        if (($Words [2] < 1) || ($Words [2] > 15))
          compile_error ('Invalid Length', $Words, 2);
        
        $Word =
          (1 << 28) |                         // Actual instruction
          (($Words [1] & 0x0F) << 24) |       // Starting-Offset
          ((($Words [2] & 0x0F) - 1) << 20);  // Number of LEDs to set
        
        break;
      // Set a number of indexed LEDs on current destination to current color
      case 'SET2':
      case 'SET3':
      case 'SET4':
      case 'SET5':
      case 'SET6':
      case 'SET7':
        trigger_error (strtoupper ($Words [0]) . ' is deprecated. Please use SETI.');
      case 'SETI':
        $Count = count ($Words);
        
        if (($Count < 2) || ($Count > 7))
          compile_error ('Invalid parameters', $Words);
        
        for ($i = 1; $i < $Count; $i++)
          if (($Words [$i] < 0) || ($Words [$i] > 15))
            compile_error ('Invalid Index', $Words, $i);
        
        $Word =
          (2 << 28) |                                       // Actual instruction
          ((count ($Words) - 1) << 24)                    | // Number of indexes
          (($Words [1] & 0x0F) << 20)                     | // First index
          ((($Count > 2 ? $Words [2] : 0) & 0x0F) << 16)  | // Second index
          ((($Count > 3 ? $Words [3] : 0) & 0x0F) << 12)  | // ...
          ((($Count > 4 ? $Words [4] : 0) & 0x0F) <<  8)  |
          ((($Count > 5 ? $Words [5] : 0) & 0x0F) <<  4)  |
           (($Count > 6 ? $Words [6] : 0) & 0x0F);
        break;
      // ...
      // ...
      // Copy LED-Settings from a given source to a given destination
      case 'COPY':
        if (count ($Words) != 6)
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 0) || ($Words [1] > 63))
          compile_error ('Invalid source-address', $Words, 1);
        
        if (($Words [2] < 0) || ($Words [2] > 63))
          compile_error ('Invalid destination-address', $Words, 2);
        
        if (($Words [3] < 0) || ($Words [3] > 15))
          compile_error ('Invalid start-offset', $Words, 3);
        
        if (($Words [4] < 1) || ($Words [4] > 16))
          compile_error ('Invalid size', $Words, 4);
        
        if (($Words [5] < 0) || ($Words [5] > 15))
          compile_error ('Invalid start-offset at destination', $Words, 5);
        
        $Word =
          (OP_COPY << 28) |                  // Actual instruction
          (($Words [1] & 0x3F) << 22) |      // Source-Address
          (($Words [2] & 0x3F) << 16) |      // Destination-Address
          (($Words [3] & 0x0F) << 12) |      // Start-Offset at source
          ((($Words [4] - 1) & 0x0F) << 8) | // Length to copy
          (($Words [5] & 0x0F) << 4);        // Start-Offset at destination
        
        break;
      // Mirror an entire strip to a destination-set
      case 'MIRROR':
        trigger_error ('MIRROR is deprecated. Please use MIRRORS.');
      case 'MIRRORS':
        if (count ($Words) != 4)
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 0) || ($Words [1] > 63))
          compile_error ('Invalid source-address', $Words, 1);
        
        if (($Words [2] < 0) || ($Words [2] > 63))
          compile_error ('Invalid destination-address', $Words, 2);
        
        if (($Words [3] < 1) || ($Words [3] > 64))
          compile_error ('Invalid size', $Words, 3);
        
        $Word =
          (OP_MIRRORS << 28) |               // Actual instruction
          (($Words [1] & 0x3F) << 22) |      // Address of strip to mirror
          (($Words [2] & 0x3F) << 16) |      // Address of first strip to mirror to
          ((($Words [3] - 1) & 0x3F) << 10); // Number of strips to mirror to
        
        break;
      // ...
      // Shift an entire set to the right
      case 'SHIFTR':
        $Count = count ($Words);
        
        if (($Count != 3) && ($Count != 5))
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 0) || ($Words [1] > 31))
          compile_error ('Invalid source-address', $Words, 1);
        
        if (($Words [2] < 1) || ($Words [2] > 16))
          compile_error ('Invalid size', $Words, 2);
        
        if ($Count == 5) {
          if (($Words [3] < 0) || ($Words [3] > 31))
            compile_error ('Invalid source-address', $Words, 3);
          
          if (($Words [4] < 1) || ($Words [4] > 16))
            compile_error ('Invalid size', $Words, 4);
        }
        
        $Word =
          (OP_SHIFTR << 28) |                               // Actual instruction
          (($Count == 3 ? 1 : 2) << 26) |                   // Number of shifts
          (($Words [1] & 0x3F) << 20) |                     // First start-address
          (($Words [2] & 0x0F) << 16) |                     // Length of shift
          ((($Count == 5 ? $Words [3] : 0) & 0x3F) << 10) | // Second start-address
          ((($Count == 5 ? $Words [4] : 0) & 0x0F) << 6);   // Length of shift
        
        break;
      // ...
      // Add a set of strips to a group
      case 'GROUPADDS':
        if (count ($Words) != 4)
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 32) || ($Words [1] > 63))
          compile_error ('Invalid group-address', $Words, 1);
        
        if (($Words [2] < 0) || ($Words [2] > 31))
          compile_error ('Invalid source-address', $Words, 2);
        
        if (($Words [3] < 1) || ($Words [3] > 31))
          compile_error ('Invalid size', $Words, 3);
        
        $Word = 
          (10 << 28) |                  // Actual instruction
          (($Words [1] & 0x3F) << 22) | // Address of group
          (($Words [2] & 0x3F) << 16) | // Address of first strip to add
          (($Words [3] & 0x3F) << 10);  // Length of add
        
        break;
      // Add a set of addressed strips to a group
      case 'GROUPADDI':
        $Count = count ($Words);
        
        if (($Count < 3) || ($Count > 5))
          compile_error ('Invalid parameters', $Words);
        
        if (($Words [1] < 32) || ($Words [1] > 63))
          compile_error ('Invalid group-address', $Words, 1);
        
        for ($i = 2; $i < $Count; $i++)
          if (($Words [$i] < 0) || ($Words [$i] > 31))
            compile_error ('Invalid address', $Words, $i);
        
        $Word =
          (11 << 28) |                                    // Actual instruction
          ((count ($Words) - 2) << 24)                  | // Number of addresses
          (($Words [1] & 0x3F) << 18)                   | // Address of group
          (($Words [2] & 0x3F) << 12)                   | // Frist address to add
          ((($Count > 3 ? $Words [3] : 0) & 0x3F) << 6) | // Second address to add
          ((($Count > 4 ? $Words [4] : 0) & 0x3F));       // Thrid address to add
      // Commit current LED-Settings
      case 'FRAME':
        if (count ($Words) > 2)
          compile_error ('Invalid parameters', $Words);
        elseif (count ($Words) == 1)
          $Words [1] = $Defines ['default_wait'];
        
        if (($Words [1] < 0) || ($Words [1] > 0xFFFF))
          compile_error ('Invalid wait-timeout', $Words, 1);
        
        $Word =
          (12 << 28) |
          (($Words [1] & 0xFFFF) << 12);
        
        break;
      // Move to a given offset of code
      case 'GOTO':
        if (count ($Words) != 2)
          compile_error ('Invalid parameters', $Words);
        
        // Check for a named mark
        if (!isset ($Offsets [$Words [1]])) {
          $Address = (int)$Words [1];
          
          if (($Address % 4 == 0) && ((($Address > 0) && ($Address <= 0xFFFF)) || ($Words [1] === '0')))
            $Word = (13 << 28) | ($Address << 12);
          else
            compile_error ('Invalid jump-address', $Words, 1);
        } else
          $Word = (13 << 28) | (($Offsets [$Words [1]] & 0xFFFF) << 12);
        
        break;
      // Set a global variable
      case 'DEFINE':
        $Defines [$Words [1]] = $Words [2];
        
        continue (2);
      default:
        // Check for a jump-mark
        if ((count ($Words) == 1) && (substr ($Words [0], -1, 1) == ':')) {
          $Offsets [substr ($Words [0], 0, -1)] = strlen ($Opcode);
          
          continue (2);
        }
        
        // Raise an error
        compile_error ('Unknown instruction', $Words);
    }
    
    // Write to output
    $Opcode .= pack ('N', $Word);
  }
  
  fclose ($fIn);
  
  // Write programme to output
  $fOut = fopen (basename ($argv [1], '.led') . '.ledc', 'w');
  fwrite ($fOut, $Opcode);
  fclose ($fOut);

?>