# PLATOTerm pro eLeMeNt ZX / MB03+

**Aktuální práce: standardní ZX verze PLATOTerm 1.2 s Wi-Fi BIOSem 2.1.**
Sestavení: `./build-standard.ps1`. Výstup je
`build-STD/PLATOTerm12_MB_EL_WIFI_alpha17.tap`.
Podrobnosti jsou v [návodu standardní varianty](standard/README.md).
Dosavadní HGFX projekt níže zůstává zachován pro pozdější pokračování.

První vývojová verze používá HGFX 3, režim 5: **512 × 384 bodů v 16 barvách**.
Logické souřadnice PLATO zůstávají 512 × 512. X se nemění, Y se převádí
na `((511-y)*3)/4`. Každý bod má vlastní barvu, bez omezení ZX atributy.

Aktuální sestavení je **alpha 4**. Při TCP chybě vypisuje konkrétní krok,
příčinu a poslední odpověď ESP, namísto společného hlášení o Wi-Fi/overflow.
Wi-Fi nyní vychází z postupu v dodané funkční `PLATOTerm_MB_EL_alpha6.tap`:
**rychlost UARTu při startu vůbec nemění**. Běžný text kreslí po řádcích a obdélníky po úsecích až
256 bodů; tučný a otočený text zatím používá původní pomalejší cestu.

Před příjmem zjišťuje čekající délku přes `AT+CIPRECVLEN?`; u prázdného
socketu nevolá `CIPRECVDATA`. Inicializační příkazy mají stejné pořadí jako
ve funkční alpha6. Při chybě připojení lze stisknout R pro opakování nebo W
pro zadání SSID a hesla. Detaily porovnání jsou v [referenčním záznamu](docs/reference-alpha6.md).
Tyto změny ještě vyžadují potvrzení na fyzickém Elementu. HGFX backend byl
při této úpravě zachován beze změn.

## Sestavení ve Windows

Je potřeba kompletní z88dk. Skript hledá `.tools/z88dk`, potom místní instalaci
`D:/zx_original/1zx/projekty/z88dk`. Cestu lze zadat explicitně nebo přes
proměnnou `Z88DK_HOME`. GNU Make ani WSL nejsou potřeba.

```powershell
.\build-hgfx.ps1 -Z88dk D:\cesta\k\z88dk
.\build-hgfx.ps1 -Z88dk D:\cesta\k\z88dk -Demo
.\build-hgfx.ps1 -Z88dk D:\cesta\k\z88dk -Test
```

Výstupy:

- `build-HGFX/PLATOTerm_MB_EL_HGFX_alpha4.tap`: terminál s Wi-Fi BIOSem 2.1.
- `build-HGFX/PLATOTerm_HGFX_test_alpha4.tap`: samostatný grafický test bez Wi-Fi.

Skript vytváří pouze jednu TAPku pro zvolenou variantu, rovnou s číslem verze.
Starší výstupy zůstávají ve složce pro případ porovnání.

V BASICu spustit `LOAD ""`. Nejprve doporučuji grafický test: má zobrazit
16 barevných pruhů, diagonály přes celou obrazovku, normální, tučný a otočený
text a uživatelský znak uprostřed. Poslední pruh je bílý, index 8 je šedý.

## Připojení

Nejprve uložit připojení k AP pomocí Wi-Fi Setup. Terminál použije BIOS 2.1
pro MB03+/Element, zapne modul a zkusí `AT` při aktuálním nastavení UARTu.
Rychlost musí být sladěna s modulem, například přes Wi-Fi Setup.
Nemění rychlost UARTu ani modulu. Host je volitelný,
výchozí je `IRATA.ONLINE`, TCP port `8005`. Lze zadat také `host:port`.

Při chybě spojení se nabídne R pro opakování a W pro zadání Wi-Fi přístupu.
Příkaz `CWJAP_CUR` přihlásí modul pro aktuální běh, neukládá heslo do flash.
Heslo se při psaní maskuje a diagnostika jej nevypisuje.

ESP firmware musí podporovat `AT+CIPRECVMODE=1`, `AT+CIPRECVLEN?` a `AT+CIPRECVDATA`.
Pasivní příjem nechává data čekat v ESP během vykreslování. Terminál zpracuje
nejvýše 128 bajtů na požadavek, odděluje binární data od odpovědí AT a ověřuje
délky. Při chybě za běhu nebo přetečení UART se zastaví s hlášením v ZX konzoli;
obnovení spojení během aktivní relace zatím není implementováno.

## Grafika a současná omezení

Podporovány jsou body, čáry, vyplněné obdélníky, mazání, režimy textu WRITE,
ERASE, REWRITE a INVERSE, otočení a tučné písmo. RGB barvy se převádějí na
nejbližší položku pevné 16barevné palety. Uživatelské fonty M2/M3 se uchovávají
jako 8 × 16; vestavěné M0/M1 zatím používají zvětšený původní font 4 × 6.
Obecná záplavová výplň `screen_paint` je v této verzi vypnutá.

EXTEND 0 přepne do původní ZX nápovědy a po návratu obnoví HGFX obraz i paletu.
EXTEND 9 zatím používá původní ZX hardcopy; tisk HGFX není implementovaný.
Budoucí režim 720 × 576 ještě implementovaný není.

Grafický backend je v `src/spectrum/hgfx.c`, Wi-Fi samostatně v
`src/spectrum/mb/`. Původní ZX/ZX-UNO backendy zůstávají dostupné přes své
Makefily. Wi-Fi backend není závislý na HGFX a je připraven k následnému
zapojení do verze se standardní ZX grafikou.

Paměťová okna HGFX: planar aperture `0x4000`, paleta `0x5800`, registry
`0x5b00`. Kreslení používá cílové offsety a jeden planar bajt, takže nemusí
držet framebuffer v běžné RAM Z80.

## Ověření

`-Test` spouští Z80 kód v z88dk-ticks se simulovaným HGFX/ESP rozhraním.
Kontroluje rozměry a okraje, 9bitové X, všechny barvy, mazání, obdélníky,
uživatelský text, ořezání otočeného/tučného textu, obnovu palety a binární
ESP-AT framing včetně limitu délky. Testy neověřují skutečný FPGA core,
monitor ani fyzické Wi-Fi spojení. TAPy vyžadují ověření na Elementu/MB03+
nebo v emulátoru s odpovídající podporou HGFX.

## Reference a původ BIOSu

- [HGFX registry a režimy — Lanex](https://wiki.ilnx.cz/doku.php/hgfx:porty_registry)
- [Příklad inicializace HGFX — Lanex](https://wiki.ilnx.cz/doku.php/hgfx:prvnigrafika2)
- `src/spectrum/mb/WifiBios21_MB_EL.a80` a `.cod`: BIOS 2.1 autorů Busy & Hood,
  převzatý beze změn z místního projektu Wi-Fi Setup `ver18/bios21_MB_EL`.
  Zdroj zachovává původní popis API i autorství. Volací můstek provede instalaci
  BIOSu před použitím jeho služeb.
