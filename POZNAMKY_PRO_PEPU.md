# PLATOTerm eZX / MB03+ (EZXMB) - 512x384 / 720x576 / 256x192, volba při startu

**v9zb-C-keyboard (23. 11. 2026, ostrá verze pro Milanův HW test):**

- Uvodni volba je spojena do jednoho dialogu: `Press 1-4`, u kazdeho rezimu
  pocet barev, vychozi PAL a `Filled-in graphics: YES` jsou inverzni. Klavesy
  1-4 a Y/N mohou volbu libovolne menit, jediny ENTER potvrdi oboji. Volby se
  Pri zmene displeje se pouzije cisty replay kvuli zmene grafickeho modulu.
  Y/N se meni jen lokalne: presny obdelnik popisu se smaze, text se vykresli
  jednou normalne/inverzne a nasledujici fyzicka bunka se smaze znovu az po
  kresleni. Tim se odstrani kopie posledniho glyphu i bily ctverec. Vychozi PAL
  a YES se oznaci bez druheho prekresleni loga. Volba 3 je o znak vpravo pod 1;
  mezera za ni byla odebrana, takze volba 4 zustala pod 2 na stejnem miste.
  Po zmene grafickeho modulu se po `con_replay` vybrany rezim a YES/NO jeste
  jednou lokalne vycisti a vykresli; replay totiz znovu vytvarel posledni glyph.
- Nasledna oprava vraci normalni obsluhu klaves na autorovo `getk()`.
  `io.c` pred dalsim blokujicim AT dotazem provede puvodnich 30 rychlych
  skenu, protoze stavovy automat z88dk `in_GetKey` pocita volani, nikoli
  snimky. Klavesa zachycena behem `CIPRECVLEN?` nebo `CIPRECVDATA` dostane
  prednost pred stazenim nebo vykreslenim dalsiho serveroveho bloku.
  Graficke ASM rutiny a pasivni ESP rezim se nemeni.
- Jeden rychly sken skonci po prvni zachycene udalosti, aby drzeni ovladaci
  klavesy v Mahjongu nevytvorilo frontu nekolika tahu pred prvnim prekreslenim.
- Odesilani pouziva `AT+CIPSENDBUF` z ESP8266 Non-OS AT v3.0.5. Po predani
  bajtu do TCP bufferu se neceka na pozdejsi `<segment>, SEND OK`; toto
  asynchronni potvrzeni parser odlisi od odpovedi nasledujiciho AT prikazu.
- Automaticke prepnuti eLeMeNt ZX na 20 MHz bylo odstraneno; program ponecha
  takt zvoleny uzivatelem/systemem. Vychozi displej PAL je ve volbe 1-4 od
  zacatku oznacen inverzni cislici 3 a inverzni oznaceni sleduje dalsi volby.
- Ostra obsluha klaves je znovu puvodni autorovo C `getk()`: nejkratsi
  overena reakce prvniho stisku. Umele vytvoreny autorepeat byl odstranen,
  protoze server opakovane klavesy stejne nevracel rychlosti lokalniho skenu.
  Rychle skeny uvnitr AT cekani a priorita zachyceneho stisku zustavaji.
  Odesilani pouziva puvodni kompatibilni AT+CIPSEND a ceka na SEND OK;
  CIPSENDBUF na nekterych ESP firmwarech s Wi-Fi BIOS 2.1 nevracel prompt.

- Ostré sestavení se vrací z `standard/keyboard-input.asm` na původní
  `standard/keyboard-input.c`. Nízkoúrovňové čtení matice (`keyboard-raw.asm` /
  z88dk `in_Inkey`), mapování PLATO kláves, grafické ASM rutiny i Wi-Fi přenos zůstávají
  beze změny. Stejný zdroj je nastaven v `BUILD.ps1` i `build.sh`.

**v9zb (měření zpoždění klávesnice):**
- Dva nové TAPy v TAP-TAR, běžný `PLATOTerm12_EZXMB.tap` je bajtově stejný jako ve v9za:
  - `PLATOTerm12_EZXMB_LATENCY.tap` - běžný přenos (AT+CIPSEND / AT+CIPRECVLEN?),
  - `PLATOTerm12_EZXMB_LATENCY_TRANSPARENT.tap` - Milanův průchozí režim.
- Pro klávesu odeslanou na server (jednu po druhé, nejvýš 12) čtyři časy ve snímcích 20 ms:
  stisk (`keyboard.c`), odesláno do ESP (`io_flush` po SEND OK; průchozí režim hned po zápisu),
  první data od serveru potom (`io_main` / `transparent.asm`), ta data nakreslena. Navíc každý
  dotaz AT+CIPRECVLEN?: trvání a kolik jich bylo mezi odesláním a odpovědí. Bez odpovědi do 5 s
  se klávesa zahodí.
- **EXTEND+L** vypíše na dva spodní řádky (ms, průměr/maximum) a začne znovu:
  `LAT n=12 ms avg/max: send 40/60 echo 220/280 draw 20/40`
  `total 280/300  polls/key 3.0  CIPRECVLEN? 20 ms  EXTEND+L`
  - send = stisk -> klávesa v ESP (AT+CIPSEND, „>“, SEND OK),
  - echo = odesláno -> první data od serveru (síť + server + čekání na další dotaz),
  - draw = kreslení přijatých dat, total = součet,
  - polls/key = dotazů AT+CIPRECVLEN? na klávesu, CIPRECVLEN? = průměrné trvání jednoho dotazu.
  Přesnost jednoho vzorku 20 ms, průměr z 12 je přesnější. Měřit na ustálené stránce (psaní do
  pole), jinak „první data“ nemusí být echo. Displej 1-3 (u 4 jsou spodní řádky mimo okno).
- Kód: `standard/lat.asm` (počítání), `standard/lat.c` (výpis); háčky jen pod `#ifdef LAT_TEST`
  v `keyboard.c`, `io.c` (3 místa: po SEND OK, kolem AT+CIPRECVLEN?, před/po kreslení) a
  `transparent.asm`. Ve variantě LATENCY_TRANSPARENT přijímací buffer 512 B místo 1 KB (jinak by
  program přesáhl E800h; pasivní příjem tam slouží jen pro první stránky).
- Model (tools/lattest): známé časy -> výpis přesně podle výpočtu, zahození po 5 s, přetečení
  FRAMES přes 65535.

**v9za (Milanova verze Z 19.11. převzatá a opravená):**
- Převzato od Milana: `standard/setup-control.asm` (nový), `io.c` (`failure`: místo nekonečné
  smyčky „Press a key to reconnect PLATOTerm.“ + restart; otázka Filled-in čte přes
  `setup_fill_getch`; bloky `#ifdef TRANSPARENT_TEST`), `standard/wifi/transparent.asm` (nový),
  `build.sh` / `BUILD.ps1` (setup-control.asm v obou programech, druhý program
  PLATOTerm12_EZXMB_TRANSPARENT_TEST.tap, v PowerShellu regex v jednoduchých uvozovkách).
- **Volba displeje znovu u Filled-in** (Milan): klávesy 1-4 kdykoli, kolikrát chce. Opraveno:
  na řádku volby zůstávalo původní číslo - nově se přepíše (`main.c display_apply`, stejné
  tabulky a hlášení o poškozeném modulu jako první volba, `display_reselect` pro asm; `gfx.c
  con_char_at`). Stejný displej 2 <-> 4: jen číslo (bez překreslení), stejná klávesa: nic.
  Poškozený modul: zpět PAL, hlášení a znovu otázka Filled-in.
- **Restart po chybě Wi-Fi / TCP** (Milan, např. server zavře nečinné spojení: „ESP: CLOSED“).
  Milanův `jp 0x7000` by spustil program se starými hodnotami inicializovaných proměnných
  (crt0 je při startu z RAM neobnovuje, jen nuluje BSS), `basic_save` by přepsal zálohu BASICu
  registry HGFX/FDMA a crt0 by si uložil hluboký zásobník (EXTEND+X by se nevrátil do BASICu);
  `cpu_fast` by si zapamatoval 20 MHz jako původní rychlost. Opraveno (`gfxmain.asm`):
  - první `basic_save` uloží i počáteční sekci DATA (3,2 KB) do URAM 26 od 400h (příznak
    `rs_done` v kódu, další volání nic nedělá);
  - `app_restart`: rychlost CPU zpět, DATA z URAM, SP = zásobník volání USR (crt0 ho drží na
    7023h - build.sh a BUILD.ps1 to kontrolují, stejně jako velikost DATA), skok na 7000h.
  - Model (tools/rstest): 3 starty - inicializovaná proměnná vždy výchozí, BSS 0, stejný SP,
    záloha BASICu z prvního startu, EXTEND+X vrací vzor.
- `TRANSPARENT_TEST` (Milanův pokus, ESP v průchozím režimu - klávesy jdou rovnou, bez
  AT+CIPSEND a dotazování AT+CIPRECVLEN?): převzato beze změny, jen ke zkoušení. Poznámky:
  v průchozím režimu se nepozná zavření spojení (žádná chyba -> žádný restart); po chybě
  `io_init` posílá AT příkazy, ty ESP v průchozím režimu nebere, dokud nedostane „+++“
  (1 s ticho, +++, 1 s) - restart v této verzi zřejmě skončí znovu chybou; přetečení FIFO (-2)
  se tiše ignoruje (ztracené bajty rozbijí proud PLATO).
- **Klávesnice**: `keyboard-input.asm` (v9u) se chová stejně jako původní C (ověřeno modelem
  na stejném sledu kláves), čtení matice (`keyboard-raw.asm`, z88dk in_Inkey) trvá desítky µs.
  Zpoždění je hlavně v přenosu: znak se na obrazovce objeví až když ho server vrátí - stisk ->
  AT+CIPSEND, čekání na „>“ a SEND OK -> cesta k serveru a zpět -> další dotaz AT+CIPRECVLEN?
  a AT+CIPRECVDATA -> kreslení. Busyho `inkey.a80` (MRS) je stejný druh čtení matice s tabulkami
  jako ROM (CS+3 = 4 apod., jiné kódy než z88dk 128/129 v keyboard.c) - zpoždění by nezměnil,
  nepoužito. Skutečné zrychlení = průchozí režim (Milanův TRANSPARENT_TEST) po vyřešení
  poznámek výše.
- Hlavní program 7000h-E355h.

**v9z (neviditelné písmeno při plné paletě):**
- HW video 19.11. (IRATA, guest, MAIN MENU, PAL): na krychli NeXT chybělo písmeno X. Paleta stránky
  PAL má 4 barvy: modrý papír, bílá, černá (krychle), červená (N) - plná. Barva X se přibližovala
  nejbližší položkou kromě papíru (pravidlo v9o) = černá -> černé X na černé krychli.
- `gfx.c page_colour`: inkoust (popředí), jehož nejsilnější složka R/G/B je >= 96, se při plné
  paletě nepřibližuje „černou“ položkou (nejsilnější složka < 48); černá jen když jiná nezbývá.
  Pro tuto stránku: zelená, žlutá, azurová, modrá, šedá -> bílá, oranžová -> červená, tmavé barvy
  dál černá. Týká se jen přibližovaných barev (PAL 4, HGFX 16 barev); přesné barvy beze změny.
- Model: snímky 512x384 a 720x576 shodné s předchozí verzí; logika výběru ověřena testem.

**v9y (nápověda):** řádek pod Filled-in (`io.c`, jen text): `For help press EXTEND (CS+SS), then 0.`

**v9x (barvy ve volbě displeje):**
- Řádky volby (`main.c display_choice`), zalomení posunuto za HGFX (63 a 60 znaků z 64, řádky
  pod nimi beze změny):
  `Press 1-4 for display: ZX 256x192 (mono), HGFX 512x384 (16col),`
  `PAL 720x576 (4col), HGFX 1:1 with CS+3,4 scrolling (16col): `

**v9w (úvodní obrazovka v 720x576 PAL):**
- Start (logo, nastavení, volba displeje, výběr serveru, text serveru) se kreslí v PAL 720x576
  místo HGFX 512x384 (`main.c`: pořadí modulů PAL, HGFX, ZX; při poškozeném modulu PAL hlášení
  „Graphics module 3 damaged (URAM 13)“).
- `display_choice`: překresluje jen při volbě jiného displeje než aktuálního - volba 3 už nic
  nepřekresluje, volby 1, 2, 4 přepnou modul, znovu logo a text (displej 4 nastavení v 512x384,
  okno od startu PLATO jako dřív). Poškozený modul volby -> zpět na PAL.
- Model (tools/contest, start v PAL): volby 1-4 OK.

**v9v (čekání FDMA pro libovolnou rychlost CPU, i po změně za běhu):**
- Čekání po přenosu FDMA (`fd_wait`, příznak „busy“ FDMA není spolehlivý) se kalibruje změřením
  v `gfx_init`. Dosud platilo jen pro rychlost při posledním přepnutí displeje; po zvýšení
  rychlosti tlačítkem EXTRA by čekání bylo krátké a CPU by sahalo do VRAM / registrů během přenosu.
- Nově: naměřená konstanta se přepočte na 3,5 MHz (`wk_cal` v `gfxmain.asm`), `fd_wait` na eLeMeNt
  ZX při každém volání přečte `e_zxi_030` a při změně přepočte čekání pro novou rychlost
  (`wk_check`, faktory 3,5/7/14/20/22/28/28/30 MHz jako `snd.asm`). Rozsah rozšířen (škála
  `g_wslow` 0-8), dřív se nad ~8,5 T/B hodnota nasytila (při 30 MHz by nestačila).
  MB03+ beze změny chování (rychlost hostitele, kontrola se nevolá).
- Zápis/obnova BASICu (EXTEND+X, `bas_fd`) čeká pevnou dobu ~5,7 ms při jakékoli rychlosti.
- Přepočet je v hlavním programu (moduly by se nevešly: okno 6 KB). Hlavní program 7000h-E07Dh,
  moduly: PAL 4951 B, HGFX3 5632 B, ZX 5344 B, okno 6047 B.
- Model (hwsim nové volby: `-fdmac N` škáluje podle `e_zxi_030`, `-spdat T kód` změní rychlost
  v čase T jako EXTRA): přepnutí na 30 MHz uprostřed kreslení - v9v 0 kolizí FDMA, v9u 18.
  Snímky 512x384 a 720x576 bajtově shodné s v9u, displej 4 a EXTEND+X OK.

**v9u (zvuky a klávesa v assembleru, pro libovolnou rychlost CPU):**
- `standard/snd.asm` místo knihovny z88dk `bit_*`: tón = obdélník na reproduktoru ULA (port FEh
  bit 4, okraj 0 jako dřív), půlperioda = smyčka 26 T na průchod, délka zadaná pro 3,5 MHz se
  **před každým zvukem** přepočítá podle aktuální rychlosti: eLeMeNt ZX `e_zxi_030` (0 3,5 / 1 7 /
  2 14 / 3 20 / 4 22 / 5-6 28 / 7 30 MHz, lze měnit kdykoli, např. tlačítkem EXTRA), MB03+ 3,5 MHz
  (CPU hostitele). Během tónu DI (stálá výška), stav přerušení se vrací.
  `screen_beep` (PLATO beep) 440 Hz 0,2 s jako `bit_frequency(.2,440)`; `snd_start` (po spojení)
  440 Hz 73 period jako `bit_play("A2")`; `keyboard_click` 11 cvaknutí ~250 T jako smyčka v C.
  Model: 3,5 MHz beep 699 788 T = 0,200 s, start 0,166 s, click ~1 ms; `e_zxi_030 = 3` (20 MHz)
  beep 3 999 200 T = 0,200 s, ~439,7 Hz.
- `standard/keyboard-input.asm` místo `keyboard-input.c` (`standard_getkey`, `standard_input_reset`):
  stejné chování - opakování počítané ve snímcích přerušení (FRAMES), tedy nezávislé na CPU: první
  opakování po 25 snímcích, dál po 4; 10 -> 13, 8 / 127 -> 12. Model: sled 17 kláves/časů dává
  stejné výsledky jako původní C. `keyboard-raw.asm` (z88dk `in_Inkey`) časování nemá.
  `keyboard.c` (převod kláves na PLATO, EXTEND, HELP) zůstává v C - žádné časování.
- Hlavní program 7000h-DF6Ah (o 1,8 KB méně: bez knihovny bit_* a plovoucí čárky). GFXTEST a
  testovací programy mají prázdné `screen_beep`.

**v9t (eLeMeNt ZX: CPU 20 MHz během PLATO):** při startu PLATO (první smazání obrazovky serverem,
`gfx.c` `con_tty_end`, po pauze s textem serveru) se na eLeMeNt ZX (`e_zxi_0FF` = Exh) nastaví
`e_zxi_030 = 03h` (20 MHz, porty 783Bh/793Bh; původní hodnota uložena) a modul se znovu
inicializuje (`active = 0` -> `gfx_init`), aby se čekání na FDMA (`fd_wait`, kalibrace) přepočítalo
pro novou rychlost (do 255 / pomalý režim stačí i pro 20 MHz). Pevné čekání 13 T/B v záznamu
displeje 4 nahrazeno kalibrovaným `fd_wait`. EXTEND+X vrací původní rychlost před obnovou BASICu
(`cpu_restore`). MB03+: beze změny (rychlost CPU určuje hostitelský počítač). Nastavení, logo a text
ze serveru běží dál na původní rychlost. Pozor na HW: časové limity Wi-Fi BIOSu (smyčky) jsou
při 20 MHz kratší, zvuk BEEP vyšší. Hlavní program 7000h-E662h.

**v9s (EXTEND+X na HW: bílá obrazovka bez hlášení):** v modelu návrat přes USR funguje (SP, IY,
oblast 5B00h-5DFFh v pořádku), na HW BASIC po návratu nevypsal ani hlášení, ani kurzor. Hlášení
`0 OK` jde přes RST 8, kde se zapojuje esxDOS (divMMC). Program přepíná stránky zóny 0 (porty
07h/27h ve výplni PAINT a HELP, nevrací se) a při návratu vracel `g_zxi_010` na původní hodnotu.
Port TextMachine (na HW ověřený) má z testů: **Ultimate Protect musí zůstat 2**, jinak se URAM
pomíchá s divRAM. Teď `basic_restore` po obnově: `g_zxi_010 = 2`, port 53h = 0 (ROM v zóně 0),
IM 1, EI. Mezipaměť zálohy přesunuta z 6600h (hned pod zásobníkem) do okna modulu E800h (před
prvním modulem a po vypnutí volné). Model: oblast celá shodná, návrat SP 6FF0h, IY 5C3Ah.
Na HW ověřit. Hlavní program 7000h-E60Fh.

**v9r (barvy po EXTEND+X):** po návratu do BASICu se nic nepřepisuje: systémové proměnné jsou
celé zpět ze zálohy (stav při `RANDOMIZE USR 28672`) a obrazovka se smaže v jejich barvách -
atributy = ATTR_P (5C8Dh), rámeček z BORDCR (5C48h). Pozor: zavaděč na začátku dělá
`BORDER 0: PAPER 0: INK 0`, takže s ním je to černé na černém. Model: oblast 5B00h-5DFFh celá
shodná, atributy = ATTR_P. Hlavní program 7000h-E5FCh.

**v9q (EXTEND+X zpět do BASICu):** dřív `hgfx_shutdown` + `exit(0)` vracelo do poškozeného
BASICu - za běhu se přepisuje 5B00h (registry HGFX; v 128K BASICu rutiny stránkování) a 5D00h-5D3Fh
(registry FDMA = uprostřed řádku 10 zavaděče a jeho proměnných). Teď `basic_save` (první příkaz
`main`) uloží 5B00h-5DFFh (768 B: stránkování 128K, systémové proměnné, kanály, program BASICu a
proměnné) přes 6600h do URAM 26 (FDMA 24) a `basic_restore` (v `hgfx_shutdown`) ji vrátí, potom
ULA obrazovka smazaná (v9q bílá; od v9r v barvách ze systémových proměnných).
`exit(0)` (crt0) vrátí SP z USR a BASIC skončí `0 OK`. Model (`tools/bastest`): vzor v 5B00h-5DFFh,
uložení, grafika + kreslení, vypnutí -> shodné kromě 5C48h, 5C8Dh, 5C8Fh, 5C90h (záměrně).
Spojení Wi-Fi (TCP v ESP) se nezavírá. Hlavní program 7000h-E603h.

**v9p (texty nastavení, na pokyn uživatele):** řádek UART připojen k řádku Wi-Fi
(`Wi-Fi for eLeMeNt ZX / MB03+. Wi-Fi BIOS 1.2, UART: 115200 Bd`), druhý řádek výzvy
`HGFX 1:1 with CS+3,4 scrolling: `, po odpovědi na `Filled-in graphics?` řádek
`For help press EXTEND, then 0.` a prázdný řádek. Rozložení pod logem: 12 Wi-Fi + UART,
13-14 volba displeje, 15 Filled-in, 16 nápověda, 17 prázdný, 18 výběr serveru (`io.c`
`select_server` nově `gotoxy(0,18)`, jen větev `__HGFX__`). Jen texty a pozice výpisu,
Wi-Fi a UART beze změny. Hlavní program 7000h-E595h.

**v9o (výřez ve třech polohách, viditelný inkoust):**
- Displej 4: CS+3 / CS+4 posouvají okno jen mezi třemi polohami - začátek (řádky PLATO 511-128),
  střed (447-64) a konec (383-0), krok 64 řádků (`ovl_hgw.asm` `win_move`). Stisk za krajní
  polohou nic nepřekresluje.
- Barvy (512x384, výřez i 720x576): když je paleta stránky plná (16 / 4 barvy) a barvu
  inkoustu (popředí) je nutné přiblížit, vybere se nejbližší položka **jiná než papír** (barva
  stránky = položka 0 a aktuální pozadí ze serveru), aby inkoust nebyl neviditelný
  (`gfx.c` `page_colour(rgb, ink)`). Přesné barvy a volné položky beze změny, pozadí beze změny.
  Model: 16 / 4 barvy obsazené + inkoust (6,6,6) na černé stránce -> index 9 (512x384) / 3
  (720x576), dřív 0 = papír. Regrese beze změny. Hlavní program 7000h-E574h.

**v9n (displej 4: HGFX 512x384 s výřezem stránky PLATO 1:1):** volba `Press 1-4` (výzva na dva
řádky: `Press 1-4 for display: ZX 256x192, HGFX 512x384, PAL 720x576,` / `HGFX 512x384 1:1 with
CS+3/CS+4 scrolling:`). V 512x384 se zmenšením 3/4 splývá každý čtvrtý řádek PLATO a smazání
jedné čáry smaže i sousední (Shanghai: kurzor maže okraje kamenů). Displej 4 kreslí stránku
1:1, vidět je 384 z 512 řádků; CS+3 posune okno nahoru, CS+4 dolů (v9o: tři polohy). Nová stránka (smazání ze serveru) začíná vždy nahoře.
- **Samostatný modul** `gfx/ovl/ovl_hgw.asm` (ID 'W', URAM 17, TAP 5. blok `hgw`, TAR
  `PT12WIN.BIN`): HGFX 3 se 16 barvami jako 512x384, text 1:1 jako PAL (`PROJ=0`),
  `tr_y = 511 - woff - y`, ořezání na okno (`CLIPY` v `base.inc`: bod, blok, čára; znaky na
  hraně okna C cestou `screen_char_draw_slow`), záznam, přehrání a posun okna. Moduly PAL, HGFX
  a ZX jsou **bajtově shodné s v9m** (ověřeno překladem obou zdrojů se stejnými symboly).
- **Záznam:** vše, co server pošle po smazání obrazovky, jde FDMA 24 do URAM 18-25 (63,5 KB,
  na konci 512 B pro stav dekodéru: proměnné `CurMem`..`FastText` + barvy RGB). Při posunu
  modul obnoví stav, smaže obrazovku a stránku přehraje přes `ShowPLATO` s `rec_play = 1`.
  Při přehrávání se serveru nic neposílá (`protocol.c`: `io_send_byte` přes `pt_send_byte`,
  jen pro `__HGFX__`). Přeplnění záznamu: posun nefunguje až do další stránky.
- **Hlavní program jen napojení:** `keyboard.c` (`__HGFX__`: kódy 128 CS+3 / 129 CS+4 ->
  `hgfx_window` v obsluze mimo příjem, jako HELP; dřív tyto kódy četly za koncem tabulky kláves
  a posílaly serveru náhodnou klávesu - teď v ostatních displejích nedělají nic),
  `protocol_asm.asm` (adresy přijatého bloku, na konci bloku v displeji 4 skok do modulu),
  `gfx.c` (režim 3, přepnutí na výřez při startu PLATO `con_tty_end`, oznámení nové stránky,
  bod výplně mimo okno se vynechá, HELP v displeji 4 s oknem nahoře a posledním řádkem výš),
  `main.c` (volba 4, chyba Wi-Fi kreslí logo zase v 512x384), `gfxmain.asm` (proměnné, skok
  `win_ext` na OVL+49), `ovlhdr.inc` (vstup +49 jen pro modul W). Nastavení, logo a text ze
  serveru se v displeji 4 kreslí v 512x384 (celé logo); výřez až od startu PLATO.
- **Rychlost:** běžné kreslení beze změny (bez tabulky 3/4); záznam ~13 T na bajt přijatých dat
  (FDMA). Posun = nové nakreslení stránky (v modelu malá stránka 0,6-0,9 MT, s velkou výplní
  2,5 MT, tj. 0,2-0,7 s při 3,5 MHz).
- **Omezení:** výplň (PAINT), jejíž bod leží mimo okno, se v té poloze okna nenakreslí (vyplní
  se po posunu, až je bod vidět). Volné stránky URAM 17-25 na HW neověřeny.
- **Ověřeno v modelu** (`tools/wintest`): rámečky + kurzor 1 bod vedle (po smazání kurzoru
  okraje celé), úhlopříčka přes stránku, text nahoře / dole / přes hranu okna, výplň, 4x dolů
  + 1x nahoru; dotaz serveru (echo 70h) odpověděl jednou (3 B), po 5 přehráních stále 3 B.
  `tools/contest` displej 4: nastavení v 512x384, po smazání ze serveru modul W. Regrese
  displejů 1-3 beze změny, zxcmp 40/40. Hlavní program 7000h-E541h, modul W 6052 B.

**v9m (512x384: přesné barvy ze serveru, 16 na stránku):** dřív pevná paleta 16 barev ve stylu
Spectra (složky 0/192/255, šedá 96) a barva ze serveru zaokrouhlená na nejbližší - chybí v ní
oranžová, středně zelená, fialová a světle modrá odlišná od azurové, takže např. duhové jablko
(6 pruhů) splynulo do ~3 barev (zelená -> šedá, oranžová -> žlutá, modrá = pozadí). Teď stejně
jako v 720x576, jen se 16 místo 4 položkami: při každém smazání obrazovky 0 = pozadí, 1 =
popředí, každá další nová barva dostane další volnou položku s přesným RGB (paleta 5800h,
3 B na položku), 17. a další = nejbližší z nastavených. HELP: bílá na černé přes paletu stránky,
po návratu paleta i barvy ze serveru zpět. Pevná paleta a `nearest()` odstraněny.
Ověřeno v modelu: 6 pruhů (barvy loga Apple) na světle modré stránce -> indexy 2-7, paleta
v 5800h přesně RGB ze serveru. Regrese 720x576 beze změny; 512x384 jen jiná čísla barev
(tvary a plochy shodné; test s 25 náhodnými barvami se po 16 přibližuje jinak). Úvodní logo
a výpis beze změny. Hlavní program 7000h-E3CAh.

**v9l (hláška chyby Wi-Fi):** z výpisu chyby (`io.c` `show_error`, jen větev `__HGFX__`, jen text,
na pokyn uživatele) zrušen řádek `PLATOTerm HGFX alpha45`. Výpis začíná `Wi-Fi error: ...`, pak
`Step: ...`, `ESP: ...` a při selhání `AT+CIPSTART` beze změny `THE SERVER SEEMS TO BE DOWN,
CONTACT THE ADMINISTRATOR` a volba R/W. Řádky `alpha21` patří staré samostatné verzi PAL a do
tohoto programu se nepřekládají (v binárce žádné „alpha“). Hlavní program 7000h-E525h.

**v9k (černé čáry a výplně v 512x384 a 720x576 - oprava):** příčinou byla úprava `fg_contrast()`
z v9f. Server si často nastaví „pozadí“ na stejnou barvu jako popředí (např. bílé), ačkoli
stránka je zelená / modrá; úprava pak popředí přepsala na černou a ta zůstala i po vrácení
pozadí - čáry, výplně (logo IRATA.ONLINE, MAIN MENU) a text byly černé místo bílých. Úprava je
celá odstraněna; volba barev je zase přesně jako ve v9e a dřív (512x384: nejbližší z 16,
720x576: 4 barvy stránky). Ověřeno v modelu: obdélník + výplň (PAINT) bílou na zelené stránce
s pozadím ze serveru bílým i zeleným - před opravou černé, po opravě bílé (obě rozlišení).
Úprava bloků `& 1FFh` z v9g zůstává (jako originál). Obrazovka „Press NEXT to begin“ tím
vyřešena není (v9f ji stejně nespravila). Hlavní program 7000h-E53Ch.

**v9j (pauza po textu ze serveru, rychlý posun v ZX):**
- Když server před přepnutím do PLATO poslal text v TTY („Connecting to host ...“, uvítání NOS),
  zůstane tento text asi **2 s** na obrazovce; teprve pak první smazání obrazovky PLATO
  (`gfx.c` `con_tty_end`, příznak `con_tty_text` z `tty.c`). Pauza `con_pause` (`console.asm`):
  100× HALT (50 Hz) při povolených přerušeních, jinak smyčka 100 × 70 000 T (v modelu 2,02 s).
  Data ze serveru během pauzy čekají ve Wi-Fi modulu (io.c beze změny). Bez textu v TTY
  žádná pauza.
- Posun textu pod logem v 256x192: řádky konzole = znakové řádky ULA, posouvají se bajty
  obrazovky (11 × 8 řádků po 32 B) místo nového kreslení znaků. V modelu test se 7 posuny
  63 -> 6,4 milionu T, obraz bajtově shodný s překreslením. V 512x384 / 720x576 se dál kreslí
  znovu (asi 0,6-0,85 s na posun; mezery se už nekreslí). Při skutečném spojení (video v9i)
  k posunu nedošlo.
- Hlavní program 7000h-E69Eh.

**v9i (TAP + TAR pro esxDOS):** složka `TAP` přejmenována na `TAP-TAR`; vedle
`PLATOTerm12_EZXMB.tap` je `PTERM12.TAR`. Uvnitř jen adresář `PTERM12` se soubory z TAPu (nic
dalšího), jména 8.3 velkými písmeny, všechny s hlavičkou +3DOS:

| soubor | blok TAPu | kam |
|---|---|---|
| `PTERM12.BAS` | zavaděč (autostart 10) | BASIC |
| `PTERM12.COD` | hlavní program | 28672 (7000h) |
| `PT12URAM.BIN` | stash (kopie do URAM) | 59392 (E800h) |
| `PT12PAL.BIN` | modul 720x576 | 16384 -> URAM 13 |
| `PT12HGFX.BIN` | modul 512x384 | 16384 -> URAM 14 |
| `PT12ZX.BIN` | modul ZX 256x192 | 16384 -> URAM 15 |
| `PTERM12.DAT` | logo (splash) | 16384 -> URAM 16 |

`PTERM12.BAS` dělá totéž co zavaděč TAPu, jen jménem: `... CLEAR 28671: LOAD *"PTERM12.COD" CODE:
LOAD *"PT12URAM.BIN" CODE:` a pro PAL, HGFX, ZX a DAT `LOAD *"..." CODE 16384: RANDOMIZE USR 59392`
(pořadí = stránky URAM 13-16), nakonec `RANDOMIZE USR 28672`. Vyrábí ho jednou
`loader/mkloader.py` (pevný soubor `loader/PTERM12.BAS`, `loader.tap` beze změny).
Data souborů jsou bajtově shodná s bloky TAPu (ověřeno). Program beze změny proti v9h.
Build (`build.sh` / `BUILD.ps1`) nově vytváří `build/PTERM12.TAR` a oba výsledky kopíruje do
`TAP-TAR/`; BUILD.ps1 používá `tar.exe` z Windows 10+. Na HW (esxDOS) zatím nezkoušeno.

**v9h (text ze serveru pokračuje pod nastavením):** dokud server nepřepne do PLATO, jde jeho TTY
text („Connecting to host - please wait ...“, „Connected“, uvítání NOS ...) do stejné konzole
jako nastavení: na další řádky pod posledním vypsaným, ve stejné mřížce řádků. Nic se nepřepisuje
a nezačíná se znovu pod logem. Po posledním řádku se text pod logem posune o řádek nahoru
(logo zůstává).
- `tty.c`: při `con_tty` jde znak přes `putch` (CR se ignoruje, LF = nový řádek); jinak beze změny.
- `gfx.c`: řádky 12-23 se drží i jako text (63 sloupců, bit 7 = inverzně) v oblasti uživatelských
  znaků 6304h-65F7h (5E00h-6303h zabírají data loga); `con_scroll` = posun textu + smazání plochy
  pod logem + nové vykreslení; `con_replay` po změně zobrazovače kreslí z tohoto textu (místo
  dřívějšího záznamu). Konec fáze: první `screen_clear` mimo kreslení loga (server přepnul do
  PLATO) - oblast znaků se vynuluje pro znaky ze serveru.
- `main.c`: `logo_busy` kolem kreslení loga a přepnutí zobrazovače (jejich `screen_clear` fázi
  neukončí); zrušeno `tty_below_logo` / `TTYLoc.y=240` z v9e. `io.c`, `terminal.c`, Wi-Fi a UART
  beze změny.
- Ověřeno v modelu ve všech třech zobrazovačích (přepnutí s obnovou textu, 12 řádků serveru ->
  posun); ZX proti originálu r1: 40/40 shodných; regrese kreslení beze změny.

**v9g (bloky jako originál):** v 512x384 a 720x576 se souřadnice vyplněného bloku mimo 0-511 nově
zalamují (`& 1FFh`) stejně jako v původní ZX verzi (a v našem režimu 256x192), místo oříznutí na
okraj. Blok s chybným vzdáleným rohem (např. ze serveru) tak nesmaže / nezakryje celou stránku.
Regrese beze změny (bloky v testu jsou v rozsahu 0-511).

**v9f (obrazovka „Press NEXT to begin“) - ve v9k ZRUŠENO (způsobovalo černé čáry):** v 256x192 vidět, v 512x384 a 720x576 černo. Server posílá
všem stejná data; režim ZX barvy ignoruje (jako originál vždy bílá na černé), HGFX 3 a FullPAL
kreslí barvou ze serveru - text v barvě pozadí (nebo tak tmavý, že se v 512x384 zaokrouhlí na
černou) nebyl vidět. Nově `fg_contrast()` v `gfx.c`: když by popředí splynulo s pozadím (512x384:
stejná z 16 barev; 720x576: barvy stránky bližší než ~40 na složku), kreslí se bíle na tmavém
a černě na světlém pozadí. Rozdílné barvy beze změny; testy kreslení beze změny. Příčinu
na serveru nelze odsud ověřit (IRATA.ONLINE není z prostředí dostupný) - na HW potvrdit.

**v9e (oprava po HW videu):**
- **Text ze serveru přes nastavení:** server píše v TTY od `TTYLoc` y = 256 po 16 bodech, řádky
  konzole mají krok 21,3 - psalo se přes sebe (ve všech zobrazovačích). (Řešení z v9e nahrazeno
  ve v9h.)
- **Pomalé mazání v 256x192:** blok (i mazání plochy pod logem) se v modulu ZX kreslil bod po bodu
  jako z88dk `draw`; teď po bajtech se stejným výsledkem (ověřeno proti originálu r1: shodné).
- **Šum při nahrávání:** zavaděč nejdřív `BORDER 0: PAPER 0: INK 0: CLS: POKE 23624,0`, takže
  bloky nahrávané do obrazovkové paměti nejsou vidět.

**v9d (názvy):** výstupy už nenesou `UNI`: `PLATOTerm12_EZXMB.tap` (dříve `PLATOTerm12_EL_MB_UNI.tap`),
`GFXTEST_EZXMB.tap`, balík `PLATOTerm-ezxmb-v*.zip`. Obsah TAPu beze změny proti v9c.

**v9b (výpis pod logem):** dva řádky spojené do jednoho `Wi-Fi for eLeMeNt ZX / MB03+. Wi-Fi BIOS 1.2`
(`io.c`, jen text), volba zobrazovače `Press 1-3 for display: ZX 256x192, HGFX 512x384, PAL 720x576`
- reaguje jen na klávesy 1-3 (bez ENTER), vypíše číslo a pokračuje; 1 = ZX, 2 = 512x384 (startovní,
bez přepnutí), 3 = 720x576. Údaj `FDMA/FDMA` za volbou vypuštěn.

**v9a (oprava loga po HW testu):** na HW se první část loga (prvních 512 B z URAM 16, čtených
procesorem přes zónu 0) načetla špatně - dekodér nedostal úvodní `ESC 02`, zůstal v TTY a zbytek
loga vypsal jako text. Logo se teď načítá **jedním přenosem FDMA 24 (URAM 16 -> 5E00h)**, stejnou
cestou jako moduly (na HW ověřené `FDMA/FDMA`), konec se pozná podle značky `EN` hned za daty
loga (blok v TAPu: 1282 B + `EN` + nuly + `EN` na konci pro stash). Čtení přes zónu 0 zůstává
jen jako záloha. FDMA 24 po přenosu vypíná FDMA, proto se po načtení loga grafika nastaví
znovu celá (`gfx_reset`). `fd24.inc` má délku přenosu v proměnné `fz_len` (stash, ovl_load,
splash_load). Záznam výstupu konzole je nově 6308h-65FBh (za kopií loga; od v9h text řádků 6304h-65F7h).
Model: start s logem shodný na eZX i MB03+; FDMA 24 celkem 32004 B (4 bloky + modul + logo).

**v9 (start jako ZX verze):** po spuštění se hned připojí 512x384 a nakreslí logo (splash),
nastavení se vypisuje pod logo - rozložení jako původní ZX verze (24 řádků přes celou výšku,
řádek 12 hned pod čarou loga). Textová obrazovka HGFX 2 zrušena; `console.asm` kreslí znaky přes
modul zvoleného zobrazovače (jeho písmo, zvýraznění serveru inverzně), `con_draw` /
`con_clear_from` v `gfx.c`. Výpis pod logem:
```
Wi-Fi BIOS 2.1 - Element / MB03+
Wi-Fi for Element / MB03+.
UART: 115200 Bd
Display 1=512x384 2=720x576 3=256x192 ZX [1]: 1  FDMA/FDMA
Filled-in graphics? Y/N [Y]: Yes
Select server (1/2, ENTER):
 1  IRATA.ONLINE
 2  cyberserv.org
```
- `io.c` (se svolením, jen výpis, žádná rutina Wi-Fi): zrušen řádek `PLATOTerm HGFX 512x384
  alpha39`, před `Filled-in graphics?` volání `display_choice()` (v `main.c`). Seznam serverů
  zůstal na svém pevném řádku 17.
- Volba 2 / 3: jiný modul, logo se nakreslí znovu a dosavadní řádky pod ním také (záznam výstupu
  od smazání řádku 12 v oblasti uživatelských znaků 5E00h, před serverem nepoužité; po volbě
  se oblast vynuluje). `FDMA/FDMA` = jak byl modul uložen / připojen (kontrola na HW).
- Chyba Wi-Fi (`hgfx_console`): logo znovu a text chyby pod ním.
- **Splash v URAM 16:** data loga (1282 B, `loader/splash.bin`) už nejsou v hlavní paměti -
  zavaděč je uloží jako čtvrtý blok (`FOR i=1 TO 4`), logo se čte do 5E00h (v9a: FDMA 24).
  Hlavní program 7000h-E327h (do okna modulu E800h volných 1241 B; bez toho by zbylo 82 B).
- GFXTEST: volba 1-3 pod logem stejnou konzolí.
- Na HW ověřit: čtení URAM 16 přes zónu 0 při startu (logo), přepnutí zobrazovače po volbě.

**v8 (třetí zobrazovač: standardní ZX 256x192):** volba `3  256 x 192  standard ZX`. Modul
`gfx/ovl/ovl_zx.asm` (5056 B, URAM 15) kreslí do obrazovky ULA (HGFX vypnuté, `g_zxi_001` = 0)
přesně jako původní PLATOTerm ZX (r1 z `build-STD`): tabulky přepočtu `scalex`/`scaley` a písmo
4x6 jsou převzaté z jeho binárky (`zxdata.inc`), body / čáry / bloky jako z88dk `plot` / `draw`
(každý bod nastaví atribut buňky na 07h), text jako upstream `screen_char_draw` (rychlá cesta
pro Write / Erase, plná pro bold, rotate, rewrite, inverse), barvy jen čisté (jinak bílá / černá),
PAINT jako z88dk `fill`, smazání = bitmapa 0 a atributy 07h. M2/M3 se z 8x16 (5E00h) zmenšují
na 4x6 algoritmem originálu až při prvním kreslení (`g_cgen` v `gfx.c` hlásí nové znaky).
HELP: kopie obrazovky 6912 B v URAM 0 přes zónu 0. Po návratu z textové obrazovky se ZX
obrazovka smaže (atributy 5800h jsou v HGFX režimech paleta).
- **Ověření proti originálu:** v modelu se pouští původní r1 a náš modul se stejným proudem
  bajtů a porovnává se obrazovka ULA po bodech včetně barev: splash, text ve 4 režimech, bold,
  rotate, M1, M2/M3 s nahranými znaky, barvy, čáry / bloky / body, smazání, ohraničené PAINT
  a 40 náhodných proudů - vše **shodné**. Převzaté jsou i zvláštnosti originálu (čtení tabulek
  za koncem pro souřadnice nad 511, malá písmena v M1).
- **Rozdíly (záměrně):** M3 znaky 60h-7Fh originál čte z vlastních proměnných (nesmyslné
  tvary), u nás prázdné. PAINT velké plochy: originál přeteče buffer výplně - nechá mezery,
  na celé obrazovce spadne (v modelu skok do ROM); u nás se plocha vyplní celá.
- Splash v ZX režimu i se startem 5,7 MT, originál 9,4 MT.
- TAP nese 3 moduly (`FOR i=1 TO 3`, URAM 13 PAL, 14 HGFX 3, 15 ZX). Hlavní program 7000h-E676h.
- Na HW ověřit: přepnutí do ULA (`g_zxi_001` = 0) a zpět do textové obrazovky na eZX i MB03+.

**v7 (dekodér protokolu v asm):** `ShowPLATO` je nově v `standard/protocol_asm.asm` (Z80).
V asm je hlavní smyčka přes přijaté bajty (TELNET IAC FF FF, TTY/ESC příznak), řídicí znaky
(BS, HT, LF, VT, FF, CR, 00h, přepnutí režimů blok / bod / čára / text), text (Alpha - buffer po
64 znacích, posun včetně bold / reverse / rotate a zalomení okraje) a souřadnice (GoCoord,
LoadCoord, blok, bod, čára). V C zůstává beze změny: režim TTY a sekvence ESC (`proto_esc` =
původní kód, jen vyjmutý do funkce), data typu slovo / barva / PAINT (`DataChar`) a ostatní
režimy `GoMode`. Stav dekodéru jsou dál proměnné z `protocol.c` (jen bez `static`).
Wi-Fi a sériové rutiny beze změny - `io.c` volá `ShowPLATO` jako dřív.
- **Splash: 4053 -> 2430 kT (-40 %)** v 512x384, 4041 -> 2418 kT v 720x576
  (eZX 2617 / 2789, MB03+ 3027 / 3673 kT). Kreslení i ostatní časy beze změny.
- **Ověření:** rozdílový test v modelu - stejný proud bajtů projde původním C dekodérem
  a novým; každé volání kreslení, odeslaný bajt, změna režimu a konečný stav všech proměnných
  dekodéru se porovnává. 41 proudů (splash ZX + 40 náhodných po 24 000 bajtech: ESC
  sekvence, TTY a návrat, IAC, souřadnice, text, barvy, PAINT, načítání znaků a paměti, echo;
  volání `ShowPLATO` po náhodně dlouhých kusech), celkem ~700 000 událostí: vše shodné.
  Test prokazatelně zachytí chybu (dvě schválně vložené chyby odhalil).
- Hlavní program 7000h-E4E1h (do okna modulu E800h zbývá ~800 B).

**v6a (převzatá oprava ERASE v 720x576 z upravené v3):** jen v modulu PAL: text v režimu ERASE
smaže celé buňky znaků obdélníkem v barvě papíru stránky (`pal_erase_chars`), ERASE/REWRITE berou
papír z `_gfx_paper`, blok v ERASE má barvu papíru stránky (`blockcol`). Proti předloze jedna změna:
načtení `_gfx_paper` přepisovalo registr A s režimem, takže v 720x576 by INVERSE kreslil jako WRITE
a REWRITE bez pozadí - režim se teď podrží v D. 512x384 beze změny. V testu se proti fast2 liší
jen snímek 005: ERASE přes REWRITE maže celé buňky (41 bodů textu REWRITE, které glyf ERASE nekryje).

**v6 (body 1 a 2):** obraz dál bod po bodu shodný s fast2 (22/22, eZX, MB03+, náhradní cesty HELP).
- **Strmé čáry od 32 bodů:** okno HGFX (96 řádků 512x384 / 68 řádků 720x576) se nastaví jednou
  na celý svůj rozsah a další řádek je jen `add hl,de` (dřív zápis D_OffY za každý bod). Bod
  takové čáry ~58 T místo ~73 T (-20 %, výpočet). Krátké strmé čáry jedou starou smyčkou,
  protože nastavení okna by u nich stálo víc, než ušetří. V testu (60 náhodných čar, většinou
  víc vodorovných) jen -1 %.
- **PAINT:** vyhledání bajtu chunky (`tval`) vložené přímo na 6 míst místo volání, levnější
  pojistka počtu úseků. PAINT 512x384 4990 -> 4804 kT, 720x576 5965 -> 5725 kT (-4 %).
  Zásobník úseků přes `ld sp` / `push` / `pop` jsem spočítal a nevyplatí se: 6 bajtů na úsek
  stojí přes (HL) stejně jako přes SP i s jeho uložením.
- Tabulka `tr_y` zmenšena na 512 B (horní bajt = 1 pro y < 170), modul 512x384 má 5632 B.

**v5 (zrychlení kreslení A-D):** grafika obou zobrazovačů zůstává v modelu bod po bodu shodná
s fast2 (22/22 snímků, i na eZX, MB03+ a u všech náhradních cest).
- **A - HELP:** uložení / obnova plátna jde FDMA 24 přímo mezi pohledem chunky (53h, typ 9) a URAM,
  bez kopírování přes CPU, po blocích 8 KB (32 řádků 512x384 / 45 řádků 720x576).
  HELP: 512x384 5173 -> 553 kT (9x), 720x576 4061 -> 610 kT (6,7x).
  Program si to při prvním HELPu sám ověří. Kontrolní bajty prvního a posledního řádku bloku napřed
  na obrazovce označí doplňkem, po přenosu je porovná a vrátí. Když FDMA 24 chunky nevidí, HELP
  jede postaru přes CPU. Když nejde zápis do chunky přes více řádků naráz, obnova jede po
  jednom řádku (~1,0 Mt). Obě náhradní cesty jsou v modelu ověřené.
- **B - čáry:** šikmé čáry sbírají body jednoho bajtu v registru a zapisují ho jednou (na hraně
  bajtu a při změně řádku); smyčky jsou přes `djnz`. Čáry -14 %.
- **C - režie volání:** tabulka `tr_y` pro 512x384 (1 KB v modulu místo násobení 3/4); `setink`
  se přeskočí, když barva a registry platí (každý jiný zápis PMODE/SCOL/PMASK/D_OffX v modulu
  i `mode_on` v C to zneplatní - `g_inkok`). Vodorovné a svislé čáry už dřív šly přes
  obdélník (`line_axial`). Text -1,5 %, body -3 %.
- **D - PAINT:** hledání konce úseku po 4 bajtech (rozbalená smyčka, celé bajty cílové barvy se
  přeskočí jedním `cp (hl)`), hledání dalšího cílového bajtu bez volání `tval`, výpočet bajtu
  a masek úseku přímo z pozice chunky (bez převodu na x). PAINT: 512x384 6329 -> 4990 kT (-21 %),
  720x576 7159 -> 5965 kT (-17 %).
- Okno modulu je teď **E800h-FFFFh (6 KB)** - modul 512x384 má s tabulkou 5888 B (ve v6 5632 B).
  Stash na E800h (`RANDOMIZE USR 59392`), registry FDMA ve stash na EA00h.
- Splash se ve v5 zrychlil jen málo: z 60 % ho dělal dekodér protokolu (ve v7 přepsán do asm).

**v4:** zobrazovače 360x288 a 512x192 jsou z programu pryč (na přání); zůstávají 512x384 (HGFX 3)
a 720x576 (FullPAL). Úvodní textová obrazovka 512x192 (HGFX 2) zůstává - je to jen konzole pro volbu
a nastavení Wi-Fi, ne kreslicí modul. TAP nese 2 moduly (URAM 13 PAL, 14 HGFX 3). Grafika obou
zobrazovačů je v modelu dál bod po bodu shodná s fast2.

**v2 (oprava po HW testu "Graphics module not in URAM"):** stash přeskočil uložení modulů, protože
počet volných stránek URAM (g_zxi_011/012) četl před zapnutím Ultimate Protect - mimo tento režim
nejspíš hlásí 0. Kontrola velikosti URAM je pryč úplně (URAM tam vždy je, vše se nahrává z TAPu
předem) - i u HELP a PAINT, kde byla stejná kontrola (HELP mohl kvůli ní zbytečně žádat DATA).
Navíc pojistka: po FDMA 24 se stránka zkontroluje přes zónu 0 (značka `EN`) a když FDMA nedoručila,
modul přenese CPU přes zónu 0. Program pod volbou vypíše `Module: stored FDMA|CPU, attached FDMA|CPU`
- tím se na HW ukáže, zda FDMA 24 RAM -> URAM a URAM -> RAM funguje.

Stav: jedna verze, jeden TAP. Model (z80ex + HGFX/FDMA) v pořádku, **na HW zatím nezkoušeno**.
Wi-Fi a sériové přenosy (`standard/wifi/*`), `protocol.c`, `terminal.c`, `keyboard.c`,
`keyboard-input.c` jsou beze změny (bajtově shodné s fast2).

## Co uživatel uvidí

1. Po nahrání se rovnou zapne HGFX 2 (512x192) a vypíše text fontem ZX ROM (64 x 24 znaků):

   ```
   PLATOTerm 1.2
   Element / MB03+

   Display:
    1  512 x 384  16 colours  (ENTER)
    2  720 x 576   4 colours  FullPAL
   Select 1-2:
   ```
2. Pak jako dřív nastavení Wi-Fi (v téže textové obrazovce, od řádku 12), po spojení přepnutí do
   zvoleného zobrazovače. Chyba Wi-Fi -> zpět do textové obrazovky (smazané), EXTEND+X -> BASIC.

## Dva kreslicí moduly

| volba | režim | rozlišení | barvy | modul | URAM | HELP |
|---|---|---|---|---|---|---|
| 1 | HGFX 3 (05h) | 512x384 | 16 pevných | `ovl_hgfx3` | 14 | kopie v URAM 0-11 |
| 2 | HGFX 5 (07h) FullPAL | 720x576 | 4 podle stránky | `ovl_pal` | 13 | kopie v URAM 0-7 |

- Každý modul je samostatně přeložené asm (`gfx/ovl/ovl_*.asm` = konstanty režimu + společné části),
  bez jediného větvení podle režimu za běhu. Na začátku každého modulu je stejná **tabulka skoků**
  (E800h + 0, 3, 6 ...); `screen_dot_draw`, `screen_line_draw`, `screen_block_draw`,
  `screen_char_draw` a ostatní vstupy hlavního programu jsou přímo adresy v tabulce
  (`gfx/gfxmain.asm`), takže volání stojí stejně jako dřív.
- **Nahrání:** oba moduly jsou v TAPu a nahrají se při startu. BASIC zavaděč (pevný,
  `loader/loader.tap`): `CLEAR 28671`, hlavní program na 7000h, pomocná rutina `stash` na E800h,
  pak 2x `LOAD ""CODE 16384` + `RANDOMIZE USR 59392` - stash každý modul z obrazovky přenese
  **FDMA 24 bit (RAM -> URAM)** do stránky 13 a 14. Pak `RANDOMIZE USR 28672`.
- **Připojení:** po volbě zobrazovače `ovl_load` přenese vybraný modul **FDMA 24 bit (URAM -> RAM)**
  do okna E800h-FFFFh (6 KB: eZX ~1 ms, MB03+ ~11 ms). Moduly jsou doplněné na 6144 B a poslední
  dva bajty jsou `EN`: konec přenosu se pozná podle nich (na eZX běží FDMA souběžně s CPU),
  pak se zkontroluje podpis `PT` na E800h+45. Když modul chybí (málo URAM), program to vypíše.
- FDMA 24 postupem z HW testů (FDMA2TST / Screen Machine): FDINIT (vyp., stránka, nulování, zap.),
  h10 = 0, neškodná 16bit sada, h11 / h14 / h17, h10 = režim, **start zápisem h00**, h10 = 0, FDMA
  vyp.; Ultimate Protect (g_zxi_010 = 2) a zařízení URAM (g_zxi_000: eZX 1, MB03+ 0) se po přenosu
  vrátí. Ve stash jsou registry FDMA na EA00h (5D00h je v tu chvíli ještě program BASICu).
- Rychlost HGFX 3 a FullPAL je stejná jako fast2 (moduly = jádro fast2 s pevnými konstantami).

## Textová obrazovka HGFX 2 (`gfx/console.asm`)

Nahrazuje z88dk konzoli do ULA na úrovni knihovny (`generic_console_printc / cls / scrollup /
set_ink / set_paper`): `printf`, `cprintf`, `putch`, `gotoxy`, `textcolor`, `clrscr` v `io.c` kreslí do
HGFX 2 **bez změny io.c**. Znak = 8 zápisů v režimu ATTR (`D_OffY` = řádek x 8), posun o řádek =
HGFX COPY (čtení = latch zdroje, zápis do cíle; dva pásy okna). Font 4x8 (`CRT_FONT_64`) se už
nelinkuje. `standard/screen-clear.c` maže řádky konzole místo přímého zápisu do ULA paměti
(io.c tam dřív psal na 4000h - pod zapnutou HGFX by to byly smetí v obraze a v paletě).

## Paměť

- `4000h-57FFh` okno HGFX, `5800h-5AFFh` paleta, `5B00h` registry HGFX, `5D00h-5D3Fh` FDMA
- `5E00h-65FFh` font M2/M3, `6600h` BUF, `6700h` T2, `6800h-6FFFh` zásobník CPU (CLEAR 28671)
- `7000h-E350h` hlavní program (konec hlídá build), `E800h-FFFFh` okno modulu (6 KB)
- URAM: 0-11 kopie plátna pro HELP, 12 zásobník úseků PAINT, 13-14 moduly

## Model (3,5 MHz, tisíce T, MB03+ bez zdržení FDMA; v4 -> v5)

| úsek testu | 512x384 v4 | 512x384 v5 | 720x576 v4 | 720x576 v5 |
|---|---|---|---|---|
| (v6: čáry 1375 / 1519, PAINT 4804 / 5725, splash 4053 / 4041) | | | | |
| splash | 4058 | 4047 | 3996 | 4033 |
| 60 čar | 1624 | 1388 | 1779 | 1525 |
| text | 9200 | 9058 | 9161 | 9107 |
| bloky | 3532 | 3533 | 1475 | 1480 |
| PAINT | 6329 | 4990 | 7158 | 5965 |
| body | 846 | 822 | 754 | 745 |
| HELP (uložení + obnova) | 5172 | 555 | 4061 | 613 |

eZX (FDMA souběžně): HELP 925 / 1004 kT; MB03+ s pozdrženým CPU (6 T/B): HELP 1892 / 2030 kT.

- 512x384 a 720x576: všech 22 snímků testu **bod po bodu shodných s fast2**.
- Celý start v modelu: stash obou modulů do URAM a připojení každého z nich - shodné.
- Textová obrazovka: barvy, inverze (výběr serveru), smazání dolní poloviny, posun - OK.

## Na HW ověřit

1. HGFX 2 (04h) - textová obrazovka: znaky v ATTR, 2 pásy přes `D_OffY`, posun HGFX COPY.
2. Stash a připojení modulu FDMA 24 na eZX i MB03+ (start h00); bez nich se program nerozjede.
3. **HELP přes FDMA 24 a chunky (v5):** uložení (URAM <- chunky, typ 9) a obnova (chunky <- URAM)
   v jednom přenosu přes více řádků. Když to HW neumí, program sám přejde na náhradní cestu -
   na HW stačí ověřit, že obraz po HELPu je celý a jak dlouho HELP trvá (rychle = FDMA).
4. Přepnutí textová obrazovka -> grafika (vždy celá inicializace: g_zxi_001 jako první, pak
   021-023, 020, registry, paleta) a zpět po chybě Wi-Fi.

GFXTEST_EZXMB.tap (od v9c není ve složce TAP-TAR, build ho dál vytváří v `build/`): stejné kroky jako
dřívější GFXTEST, volba zobrazovače při startu; po každém kroku vlevo dole `T krok snímky`
(u kroku 2 i `W` = kalibrace čekání FDMA), další krok klávesou.

## Build

`powershell -ExecutionPolicy Bypass -File .\BUILD.ps1 -Z88dk D:\cesta\k\z88dk`
(přeložený z `build.sh`, kterým se stavělo a testovalo; PowerShell zde nebyl k dispozici).
Postup: hlavní program -> z jeho mapy `gfx/ovl/mainsyms.inc` (adresy proměnných pro moduly) ->
3 moduly (z80asm, org E800h) + logo -> stash -> TAP = `loader/loader.tap` + bloky z appmake;
TAR = `loader/PTERM12.BAS` + tytéž bloky jako soubory s hlavičkou +3DOS; oba do `TAP-TAR/`.
`loader/mkloader.py` jen jednou vyrobil pevné `loader/loader.tap` a `loader/PTERM12.BAS`.
`standard/help.c` (nápověda pro ULA, nepoužívaná) vypadl; `keyboard.c` se překládá zvlášť
s `-D__PAL__` jen kvůli volání indikace EXTEND (pruhy ve FullPAL, jinde nic).

Kosmetika: řádek "PLATOTerm HGFX alpha45" ve výpisu chyby Wi-Fi zrušen ve v9l (se svolením).
