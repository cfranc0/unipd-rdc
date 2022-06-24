# Reti di Calcolatori

Università di Padova, A.A. 2021/2022, Prof. Zingirian N. - 
🔗 [Link didattica](https://didattica.unipd.it/off/2019/LT/IN/IN0508/000ZZ/INP8084335/N0)

> Se trovate alcun **problema** nel codice, aprite una [Issue](https://github.com/cfranc0/rdc/issues) o [PR](https://github.com/cfranc0/rdc/pulls).
>
> If you find any **problem** with the code, please file an [Issue](https://github.com/cfranc0/rdc/issues) o [PR](https://github.com/cfranc0/rdc/pulls).

## Contenuto della repo

In questa repo sono contenuti i programmi sviluppati dal professore durante il corso tenuto nell'A.A. 21/22 e riscritti per essere più chiaramente comprensibili e più estensivamente commentati.
* Client web HTTP/1.1
  * Risoluzione degli hostname in IP
  * Parsing degli header
  * Supporta il `Transfer-Encoding: chunked`
* Server web
  * Invio di file statici nella cartella del server
  * `Autorization` richiesta nella cartella `secure` tramite basic HTTP auth 🔗 [RFC2617](https://datatracker.ietf.org/doc/html/rfc2617)
  * Invio tramite `Transfer-Encoding: chunked`
* Proxy web
  * Modalità `GET` in chiaro
  * Modalità `CONNECT` tramite TLS tunnelling
* Socket web server
  > Compilare con: `gcc wsock.c -o wsock -lcrypto`
  * Handshake socket
  * Comunicazione bidirezionale tra server e client

## Standard di riferimento (Standards)

* RFC1945 **HTTP 1.0** 🔗 [rfc-editor.org](https://www.rfc-editor.org/rfc/rfc1945)
* RFC2616 **HTTP 1.1** 🔗 [rfc-editor.org](https://www.rfc-editor.org/rfc/rfc2616)
* RFC3875 **Common Gateway Interface (CGI)** 🔗 [rfc-editor.org](https://www.rfc-editor.org/rfc/rfc3875.html)
* RFC6265 **Cookies** 🔗 [rfc-editor.org](https://www.rfc-editor.org/rfc/rfc6265)
* RFC6455 **WebSocket** 🔗 [rfc-editor.org](https://www.rfc-editor.org/rfc/rfc6455.html)

---

## English 

This repo contains codes developed during the [Computer Networks 1](https://en.didattica.unipd.it/off/2019/LT/IN/IN0508/000ZZ/INP8084335/N0) course at the University of Padua during the 2021/2022 academic year.

You can find scripts related to the HTTP standard as listed in the *Standards* section.

Base scripts are written in english and commented, exam solutions may be commented in italian.
