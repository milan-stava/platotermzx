# Standardni PLATOTerm 1.2 pro Element / MB03+

Aktualni sestaveni: **alpha 17**, standardni ZX grafika 256 x 192.
Zaklad je PLATOTERMZX_1_2 (tschak909/platotermzx),
commit `4250f74281931aa3cd3b77ef854dff122c8e842c`.
Originalni soubory v `upstream/` jsou zachovane beze zmen.

Uzivatel schvalil alpha 17 jako vychozi standardni verzi pred pokracovanim
na HGFX 512x384/16 barev. Podle jeho zpetne vazby autor serveru po zhlednuti
videa potvrdil dobrou rychlost. Jde o uzivatelske overeni, nikoli mereni
maximalni propustnosti serveru.

Alpha 17 opravuje obdelnikove kresleni: oba krajni radky jsou zahrnuty,
poradi rohu se normalizuje a Write/Rewrite vyplnuje misto mazani.
Pomale kresleni textu uz nemeni souradnice predane volajicim; jejich posun
zajištuje protokol nebo TTY. Opraven je i zapis atributu pozadi znaku.
Pixelovy test overuje vsechny ctyri rezimy, poradi rohu i zachovani okoli.
Nasledny zaznam skutecne odpovedi IRATA potvrdil, ze sipku po NEXT maze
primo server. Nejde o chybu klienta; podrobnosti v
[rozboru sipky](../docs/login-arrow.md). Zbytky textu na fotkach nejsou timto
zjistenim vysvetlene a zustavaji samostatnym bodem k overeni.

Alpha 15 nabizi inverzni vyber IRATA.ONLINE / cyberserv.org (port 8005).
Vychozi je IRATA, 1/2 vybere polozku, ENTER potvrdi.
Od alpha 16 SPACE vyber nemeni.
Pri prvnich datech odstrani Wi-Fi informace ve spodni polovine obrazovky.
Posledni radek se pak jednou smaze pri prvnim PLATO echo dotazu, po uvodnim
znaku A. Pozdejsi echo dotazy obsah obrazovky nemazou. Pri selhani CIPSTART
pribyla hlaska THE SERVER SEEMS TO BE DOWN, CONTACT THE ADMINISTRATOR;
podrobna chyba ESP zustava dostupna. Neni to dukaz vypadku serveru.
Uzivatel potvrdil, ze drive hlasene selhani bylo vypadkem serveru.
Kliknuti klaves v napovede z alpha 14 zustava zachovane.

Alpha 13 urychluje prevod stazenych fontu a maze spodni polovinu obrazovky
primym zapisem do videopameti. Podrobnosti a omezeni mereni jsou
v [poznamkach k vykonu](../docs/standard-performance.md).

Alpha 12 odstranila docasny pametovy zaznam odesilanych znaku, jeho
assemblerovy obal, pomocne odposlechy COM/prohlizece/TCP a souvisejici navody.
Normalni chybova hlaseni pripojeni zustavaji. Uspesne prihlaseni alpha 11
uzivatel potvrdil; odmitane heslo souviselo s mapovanim numericke klavesnice
ve webovem terminalu na PC.

EXTEND 0 pouziva lokalni help.c: vstup primo z klavesnice bez zavislosti
na ROM LAST_K (23560), omezena delka vstupu, mazani a spravny navratovy typ
vyhledavani. Otevreni napovedy je odlozeno do hlavni smycky po dokonceni AT
transakce. Opakovany EXTEND zustava lokalni a napoveda funguje i v TTY rezimu.

## Sestaveni

Spustit `./build-standard.ps1`; testy `./build-standard.ps1 -Test`.
Dalsi kontrola: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/bios-bridge.ps1`.

Vznikne `build-STD/PLATOTerm12_MB_EL_WIFI_alpha17.tap`. Starsi sestaveni
zustavaji pro porovnani. Z88dk lze zadat pomoci parametru -Z88dk.

## Pouziti

1. Wi-Fi Setup musi mit sladenou rychlost UARTu a ESP. Terminal ji nemeni.
2. Nacist TAP prikazem LOAD "". Filled-in graphics Y/N ma vychozi Yes.
3. Vybrat IRATA.ONLINE nebo cyberserv.org, potvrdit Enterem.
   R opakuje spojeni, W umoznuje zadat Wi-Fi SSID/heslo pro aktualni beh.
4. Po tonu A2 zacne prijem; Wi-Fi panel se vycisti pred serverovym textem.
   Pri uvodnim echo dotazu se jednou vycisti posledni radek.
5. Pro napovedu stisknout CAPS SHIFT + SYMBOL SHIFT, pustit a stisknout 0.
   Zadat nazev klavesy (napr. NEXT) a Enter. KEYS znovu zobrazi seznam.
   Enter v terminalu je NEXT, DELETE je ERASE.

## Implementace a overeni

Inicializace a prijem pouzivaji Wi-Fi BIOS 2.1 MB/EL, nikoli variantu Next.
TX zapisuje primo na UART port 133B s kontrolou obsazeni a timeoutem.
ESP pouziva pasivni TCP prijem, bloky do 1024 bajtu a kratke useky prijmu
pro prubezne cteni klaves. Klavesy cekaji v 64bajtove odesilaci fronte.
Docasny archiv klaves/hesla v RAM uz neni soucasti programu.

Automaticke Z80 testy overuji inicializaci ESP, RX/TX, login/group/heslo
se syntetickymi udaji, tabulky klaves, opakovani, odlozenou napovedu,
jeji vstup/mazani/limity a hlavni smycku. Instrukcni test mustku pouziva
skutecnou sestavenou binarku: BIOS RX, prime TX, vsech 256 hodnot, timeouty,
hranice bufferu a zachovani IX/IY/SP. Nejde o test na fyzickem Elementu.

Protokol vychazi z 1.2; lokalni protocol.c pridava pouze oznameni prvniho
echo dotazu pro vycisteni stavoveho radku. Lokalni screen.c pridava volbu vyplni.
Pri chybe spojeni za behu je stale nutny restart. HGFX projekt v src/
a build-hgfx.ps1 je uchovan pro dalsi praci; kontrolni soucty jsou
v preserved-sources.json.
