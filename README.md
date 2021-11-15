# AWIT
***“AWIT”*** which stands for, ***“Algorithm written in Tagalog”***, complete with its own syntaxes, and built-in virtual machine whose main purpose is the accurate and precise analysis of algorithms that exist. However, it will not be strictly in Tagalog but in the Filipino language and the reason for the naming is to preserve the Filipino word, *“AWIT”* instead of *“AWIF”*. This will be a great educational programming language for Filipino freelancers, students or professional developers who have interest in both algorithms and programming languages.

## Appreciation
The language implementation was based on [LOX's clox implementation](https://github.com/munificent/craftinginterpreters/). From the scanner to the compiler to the VM heck even the Garbage Collector. Although there are a lot of significant differences, the implementations were highly inspired by [Nystrom's Crafting Interpreters](https://craftinginterpreters.com). Again, thank you!

## AWIT ay?
- Scripting language
- High-level language (follows C-style programming)
- Dynamically typed
- Stack-based
- Interpreted (it's actually a hybrid interpreter since the implementation first compiles to bytecode then be interpreted by the Virtual Machine)
- Managed language (AWIT has its own Garbage Collector)
- Finally, it is Turing-complete

## Mga Halimbawa
Kamusta, Daigdig!:
```
ipakita "Kamusta, Daigdig!";
```

Recursive program to print the 69th number in fibonacci sequence:
```
gawain fibonacci(n) {
  kung (n < 2) ibalik n;

  ibalik fibonacci(n - 2) + fibonacci(n - 1);
}

ipakita fib(35); // This will take a couple of seconds.
```

## Panimulang Hakbang
### Mga Kailangan:
#### Windows
- [Git](https://git-scm.com/downloads/win)
- [Make and C compiler](https://github.com/mstorsjo/llvm-mingw/releases/tag/20211002) llvm-mingw-20211002-msvcrt-i686.zip
> We recommend llvm-mingw for the **make** and **C compiler** but you are free to choose your preffered **make** and **C compiler**.
#### Linux
- [Git](https://git-scm.com/download/linux)
- [Make](https://www.gnu.org/software/make/manual/make.html)
- [C compiler](https://gcc.gnu.org/))
#### Or you can just get the latest release:
- for **WINDOWS**-based systems [AWIT v0.9.8](https://github.com/Philiks/AWIT/releases/download/v0.9.8/awit_windows.zip)
- for **LINUX**-based systems [AWIT v0.9.8](https://github.com/Philiks/AWIT/releases/download/v0.9.8/awit_linux.tar.xz)

### Compilation
```
$ git clone https://github.com/Philiks/AWIT.git
$ cd AWIT
$ make
```
*or*
```
$ git clone https://github.com/Philiks/AWIT.git
$ cd AWIT/src
$ make
```
*you can run `make` either in [AWIT](./) or in [AWIT/src](/src)*
> **Note:** The `awit` or `awit.exe` is located at the [AWIT/src](/src) after compilation.

### Paandarin
- `./awit [*.awit file]` (in LINUX-based systems)
- `awit.exe [*.awit file]` (for Windows system)
```
$ src/./awit mga\ halimbawa/kamustaMundo.awit
Kamusta, Mundo!
```
*or use our REPL*
```
$ src/./awit
> ipakita "Kamusta, Mundo!";
Kamusta, Mundo!
>
```
> **Note:** Running without the file as argument will fire up the REPL.

## Mga Katangian
### Data Types
- #### Booleans
`tama` o `mali`
```
kilalanin singleKaBa = tama;
```

- #### Numbers
`6.9` `420`
```
kilalanin decimal = 6.9;
kilalanin integer = 420;
```

- #### Strings
`"Isa akong lupon ng mga salita."` `""`
```
kilalanin pangalan = "Felix Raffy Mark";
```

- #### Null
`null` (almost used `wala` but `null` prevails)
```
// unnecessary ' = null' since uninitialized
// variables are assigned with null.
kilalanin wala = null;
```

### Mga Ekspresyon
- #### Arithmetic
Token | Name
----- | ---------------------------
  `+` | addition
  `-` | subtraction
  `*` | multiplication
  `/` | division
  `%` | modulo division (remainder)
```
1 + 2 - 3 * 4 / 5 % 6; // This results to 1 btw.
```

- #### Unary
`-` negation
```
-10;
-(10 - 11); // 1
```

- #### Comparison
Token | Name
----- | ---------------------
  `<` | less than
  `<=`| less than or equal
  `>` | greater than
  `>=`| greater than or equal
```
kung (10 < 20) {
  ipakita 30;
}
```

- #### Equality
- `==` equal to
- `!=` not equal to
```
"tayo" != "bagay";
```

- #### Logical Operators
Token | Name
----- | ---------------------
 `!`  | not
 `at` | and
 `o`  | or
```
tama o mali; // tama, 'o' at 'at' are short-circuit operators
```
> For more reading about [short-circuit evaluation](https://en.wikipedia.org/wiki/Short-circuit_evaluation)

- #### Precedence and Grouping
**Operator Precedence**
Token              | Name
:----------------: | ---------------------
`.` `()`           | call
`++` `--`          | post-increment
`!` `-` `++` `--`  | pre-increment
`*` `/` `%`        | factor
`+` `-`            | term
`<` `>` `<=` `>=`  | comparison
`==` `!=`          | equality
`at`               | and
`o`                | or
`=`                | assignment

> **Note:** You can always override precedence by using groupin `()`.

### Mga Pahayag
- #### Ipakita
Syntax `ipakita <ekspresyon> ;`
```
ipakita "Kamusta ka naman?";
```

- #### Kilalanin (Declaration / Definition)
*declaration*<br />
Syntax `kilalanin <identifier> ;`
```
kilalanin walangHalaga;
ipakita walangHalaga; // null
```

*definition*<br />
Syntax `[kilalanin] <identifier> = <ekspresyon> ;`
```
kilalanin mayHalaga = tama;
ipakita mayHalaga; // tama
mayHalaga = 12;
ipakita mayHalaga; // 12
```

- #### Kung [Kundiman]
Syntax `kung ( <ekspresyon> ) <pahayag> [kundiman <pahayag]`
```
kilalanin edad = 16;
kung (edad < 18)
  ipakita "Ikaw ay menor de edad.";
kundiman
  ipakita "Isa ka ng matandang nilalang.";
```

- #### Suriin [Kapag] [Palya]
Syntax `suriin ( <ekspresyon> ) { [kapag <ekspresyon> : <pahayag>]+ [palya : <pahayag>] }`
```
kilalanin numero = 2;
suriin (numero) {
  kapag 1:
    ipakita "Isa";
  kapag 2:
    ipakita "Dalawa";
  palya:
    ipakita "Di makita ang numero.";
}
```

- #### Habang
Syntax `habang ( <ekspresyon> ) <pahayag>`
```
// Prints 0 - 9
kilalanin n = 0;
habang (n < 10)
  ipakita n;
```

- #### Gawin-Habang
Syntax `gawin <pahayag> habang ( <ekspresyon> ) ;`
```
// Prints 0 - 10
kilalanin n = 0;
gawin
  ipakita n;
habang (n < 10);
```

- #### Kada
Syntax `kada ( <ekspresyon> ; <ekspresyon> ; <ekspresyon> ) <pahayag> `
```
// Prints 0 - 9;
kada (kilalanin n = 0; n < 10; n++) {
  ipakita n;
}
```

- #### Itigil
Syntax `itigil ;`
```
kilalanin ctr = 0;
habang (tama) {
  kung (ctr == 1)
    itigil;

  ipakita ctr;
  ctr++;
}
```
> **Note:** Can only be used inside looping statements `habang` `gawin-habang` `kada`.

- #### Ituloy
Syntax `ituloy ;`
```
// DANGER!!! This will cause an infinite loop DO NOT TRY.
// Pero ikaw bahala ;)
kilalanin ctr = 0;
habang (tama) {
  kung (ctr == 1)
    ituloy;

  ipakita ctr;
  ctr++;
}
```
> **Note:** Can only be used inside looping statements `habang` `gawin-habang` `kada`.

- #### Ibalik
Syntax `ibalik [<ekspresyon>] ;`
```
gawain pagsamahin(a, b) {
  ibalik a;
}

ipakita pagsamahin(34, 35); // 69
```
> **Note:** Can only be used inside functions `gawain`.

- #### Mga Pahayag
Syntax `{ [<pahayag>]* }`
```
kilalanin a = "labas";
{
  ipakita a; // labas
  kilalanin a = "loob";
  ipakita a; // loob
}
```

- #### Ekspresyong Pahayag
Syntax `<ekspresyon> ;`
```
kilalanin ctr = 0;
ctr++; // Useful
12; // Useless
```

- #### Gawain
Syntax `gawain <identifier> ( [<identifier> ,]* ) { <pahayag> }`
```
gawain kamusta() {
  ipakita "Kamusta!";
}

kamusta();
```

- #### Closures
Syntax `gawain <identifier> ( [<identifier> ,]* ) {
  <gawain> }`
```
kilalanin tagapalit;
kilalanin tagatingin;

gawain numero() {
  kilalanin numero = 69;
  
  gawain palitan() {
    numero = 420;
  }

  gawain patingin() {
    ipakita numero;
  }

  tagapalit = palitan;
  tagatingin = patingin;
}

tagatingin(); // 69
tagapalit();
tagatingin(); // 420
```

- #### Uri
Syntax `uri <identifier> { <pahayag>* }`
```
uri Tao {
  kain() {
    ipakita "Kumakain wag guluhin.";
  }

  tulog() {
    ipakita "Tulog wag magdabog.";
  }

  laro() {
    ipakita "Laro bago palo.";
  }
}
```
*AWIT is dynamically-typed language therefore you cant define a field after instantiation.*
```
uri Programmer {}
kilalanin programmer = Programmer();
programmer.puyat = tama;
```

- #### Instantiation and Initialization
*instantiation*<br />
Syntax `kilalanin <identifier> = <class-name>( [<ekspresyon> ,]* ) ;`
```
kilalanin tao = Tao();
tao.kain();
tao.tulog();
tao.laro();
```

*initialization*<br />
Syntax `sim ( [<identifier> ,]* ) { <pahayag> }`
```
uri Tao {
  sim(pangalan, kasarian) {
    ito.pangalan = pangalan;
    ito.kasarian = kasarian;
  }
}
```  
> **Note:** `sim` is short for `simula`.
> **Note:** You cannot use `ibalik` with a value beside it inside `sim`.
> **Note:** You don't have to put the fields inside the `uri` just prefix it with `ito.`.

- #### Inheritance
Syntax `uri <identifier> < <class-name> { <pahayag> }`
```
uri Estudyante < Tao {
  sim(pangkatTaon, pangalan, kasarian) {
    mula.sim(pangalan, kasarian);
    ito.pangkatTaon = pangkatTaon;
  }

  uwi() {
    ipakita "Uwian na!";
  }
}

kilalanin felix = Estudyante("BSCS 4A", "Felix", "Lalaki");
felix.kain();
felix.laro();
felix.uwi();
felix.tulog();
```
> **Note:** You can user `mula` to access parent's fields and methods.

### Katutubong Gawain (Native Functions)
- #### oras()
*Returns* the current time in seconds in type `double`.

- #### basahin()
*Reads* an input from the user from the STDIN.
*Returns* `double` if the input is a number. Otherwise it will return `string`.

- #### mayKatangian(<class-name>, <field-name>)
*<class-name>* `uri` the class instance in which the field-name will be searched.
*<field-name>* `string` the field to be searched.
*Returns* `tama` if found. Otherwise it will return `mali`.

## Reserved Words
AWIT have 22 reserved words and they are:<br />
`at`, `gawain`, `gawin`, `habang`, `ibalik`, `ipakita`, `itigil`, `ito`,
`ituloy`, `kada`, `kapag`, `kilalanin`, `kundiman`, `kung`, `mali`, `mula`,
`null`, `palya`, `suriin`, `o`, `tama`, `uri`

## Lexical grammar
#### &lt;token&gt;
::= [&lt;keyword&gt;](#keyword)<br />
&emsp;| [&lt;identifier&gt;](#identifier)<br />
&emsp;| [&lt;constant&gt;](#constant)<br />
&emsp;| [&lt;string-literal&gt;](#string-literal)<br />
&emsp;| [&lt;punctuator&gt;](#punctuator)<br />
&emsp;;<br />
#### &lt;keyword&gt;
::= 'at' | 'ituloy' | 'mula'<br />
&emsp;| 'gawain' | 'kada' | 'null'<br />
&emsp;| 'gawin' | 'kapag' | 'palya'<br />
&emsp;| 'habang' | 'kilalanin' | 'suriin'<br />
&emsp;| 'ibalik' | 'kundiman' | 'o'<br />
&emsp;| 'ipakita' | 'kung' | 'tama'<br />
&emsp;| 'itigil' | 'mali' | 'uri' | 'ito'<br />
&emsp;;<br />
#### &lt;identifier&gt;
&emsp;| [&lt;alphabet&gt;](#alphabet) [&lt;alpha-numeric&gt;](#alpha-numeric)[\*](#kleene-star) ;<br />
#### &lt;alpha-numeric&gt;
::= [&lt;alphabet&gt;](#alphabet) | [&lt;digit&gt;](#digit) ;<br />
#### &lt;alphabet&gt;
::= 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'g' | 'h' | 'i' | 'j' | 'k' | 'l' | 'm'<br />
&emsp;| 'n' | 'o' | 'p' | 'q' | 'r' | 's' | 't' | 'u' | 'v' | 'w' | 'x' | 'y' | 'z'<br />
&emsp;| 'A' | 'B' | 'C' | 'D' | 'E' | 'F' | 'G' | 'H' | 'I' | 'J' | 'K' | 'L' | 'M'<br />
&emsp;| 'N' | 'O' | 'P' | 'Q' | 'R' | 'S' | 'T' | 'U' | 'V' | 'W' | 'X' | 'Y' | 'Z'<br />
&emsp;;<br />
#### &lt;digit&gt;
::= '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' ;<br />
#### &lt;constant&gt;
::= [&lt;number-constant&gt;](#number-constant) | [&lt;boolean-constant&gt;](#boolean-constant) | 'null' ;<br />
#### &lt;number-constant&gt;
::= [&lt;whole-number&gt;](#whole-number) | [&lt;fractional-number&gt;](#fractional-number) ;<br />
#### &lt;whole-number&gt;
::= [&lt;digit&gt;](#digit)[+](#plus) ;<br />
#### &lt;fractional-number&gt;
::= [&lt;digit&gt;](#digit)[\*](#kleene-star) '.' [&lt;digit&gt;](#digit)[\*](#kleene-star) ;<br />
#### &lt;boolean-constant&gt;
::= 'tama' | 'mali' ;<br />
#### &lt;string-literal&gt;
::= \`"\` [&lt;character&gt;](#character)[+](#plus) \`"\` ;<br />
#### &lt;character&gt;
::= [&lt;escape-sequence&gt;](#escape-sequence)<br />
&emsp;| /\* Any character on the source code except double-quotation (") and backslash (&#92;) \*/<br />
&emsp;;<br />
#### &lt;escape-sequence&gt;
::= '\"' | '&#92;&#92;'<br />
&emsp;| '\a' | '\b' | '\f' | '\n' | '\e' | '\r' | '\t' | '\v'<br />
&emsp;;<br />
#### &lt;punctuator&gt;
::= '(' | ')' | '{' | '}' | '[' | ']' | '.'<br />
&emsp;| '++' | '--' | '+' | '-' | '!'<br />
&emsp;| '&#92;' | '/' | '\*' | '%' | '&lt;' | '>' | '&lt;=' | '>=' | '==' | '!='<br />
&emsp;| ',' | ';' | ':' | '='<br />
&emsp;;<br />

## Authors
This language is all thanks to these people:<br />
[Felix Janopol Jr.](https://github.com/Philiks)<br />
[Raffy Wamar](https://github.com/waffy-kun)<br />
[Mark Julius Mella](https://github.com/Markmella)

## License
This project is licensed under the MIT License - see the [LICENSE](./LICENSE) file for details.
