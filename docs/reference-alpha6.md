# Reference Wi-Fi pro Element / MB03+

Uživatel dodal funkční `PLATOTerm_MB_EL_alpha6.tap` ze složky
`D:/Windows_Data/Stažené soubory`. Vstupní soubor zůstal beze změn.

- Délka: 35 899 bajtů.
- SHA-256: `144086B061630A107B6E481CFA18993CF10E7037233F32C76C016985A4A558DC`.
- TAP obsahuje BASIC loader, obrazovku 6912 bajtů na `0x4000` a program
  28 860 bajtů na `0x8000`.
- Srovnána byla také uživatelem určená upstream verze
  [PLATOTERMZX_1_2](https://github.com/tschak909/platotermzx/releases/tag/PLATOTERMZX_1_2),
  commit `4250f74281931aa3cd3b77ef854dff122c8e842c`.
  Tato verze obsahuje standardní síťové/RS232 rozhraní, nikoli dodatečný MB/EL
  Wi-Fi ovladač z uživatelovy TAPky. Nejde proto o prostou záměnu upstream souboru.

## Pozorování z disassemblování dodané TAPky

- `0xBAE2`: instalační můstek zachová IX a IY, zavolá instalaci BIOSu,
  zapnutí Wi-Fi a vyprázdnění příjmu. Nenastavuje rychlost UARTu.
- `0xBAF5`: příjem zachová IX/IY; vrací `-1` pro prázdný přijímač,
  `-2` při plném FIFO a jinak přijatý bajt.
- `0xBB0D`: vysílací můstek rovněž zachová IX/IY. Jeho číselná konvence
  úspěchu je jiná než v našem C rozhraní, proto nebyl slepě zkopírován.
- `0xB46B`: konfigurace obsahuje v tomto pořadí ATE0, CIPCLOSE=5,
  CIPCLOSE, CWMODE_CUR=1, CIPMUX=0, CIPMODE=0, CIPDINFO=0, CIPRECVMODE=1,
  následované CIPSTART. Chyba při zavírání neexistujícího spojení je tolerována.
- `0xB68C`: odkaz na `AT+CIPRECVLEN?`; příjem tedy nejprve zjišťuje délku,
  místo slepého požadavku na pevné množství dat.
- `0xB8A9`: zadávání SSID a hesla, přihlášení pomocí CWJAP_CUR.
- `0xB966`: volby R=retry a W=Wi-Fi při chybě připojení.
- `0xE59D`: vložený BIOS; instrukce na `0xE5E7` vrací verzi `0x0200`.

## Převzetí do HGFX alpha 4

Wi-Fi postup je nově implementován ve zdrojové podobě podle těchto pozorování.
Není to binární přenesení celého původního programu. Zachováváme BIOS **2.1**
Busy & Hood podle uživatelova předchozího zadání a naše C rozhraní.

Převzato: ponechání rychlosti UARTu, pořadí konfigurace, délkový dotaz před
čtením, možnost znovu zadat Wi-Fi přístup a opakovat připojení, ochrana IX/IY.
Z příjmu nadále předáváme PLATO pouze dokončené binární bloky po skončení AT
transakce, nejvýše 128 bajtů. Startovní FIFO se vyprazdňuje omezeným softwarovým
čtením; hardwarový reset z reference nebyl vrácen kvůli předchozí regresi alpha 2.
Obnovení spojení za běhu zatím převzato není.

SHA-256 `src/spectrum/hgfx.c` před i po změně Wi-Fi:
`49EDF21813079EC29BD6D634D1F751366767A071FF97841B72CE36E3372422C7`.

## Ověření a jeho hranice

Z80 testy kontrolují, že start nevykoná zápis rychlosti ani při neznámém
readbacku, že příkazy odpovídají zjištěnému pořadí a že prázdný socket vyvolá
pouze délkový dotaz. Ověřují také binární data, oba podporované formáty hlavičky,
uvozování Wi-Fi řetězců, vysílací prompt a limity délek.

Funkčnost reference na hardware je informace od uživatele. Nové HGFX sestavení
ještě nebylo na fyzickém Elementu/MB03+ ověřeno.
