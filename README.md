# rpiweatherd
<code>rpiweatherd</code> is a small daemon for the Raspberry Pi, written in C99, that records weather data using temperature sensors.
It can be used as a weather data server, returning JSON-formatted data via a normal HTTP request.

The daemon is designed to be small, efficient and portable, so even though it has some dependencies,
most of them are probably already installed on your Raspberry Pi.

## Repository Structure
The repository contains all code and submodules needed to successfully compile and run the daemon.

|Directory|Description|
|---------|-----------|
|<code>src/</code>|Source directory containing C source files|
|<code>include/</code>|C Header files|
|<code>skel/</code>|Templates for configuration files and initialization scripts/units|
|<code>devices/</code>|Device drivers|
|<code>extra/</code>|Administration script and an Example CLI client|
|<code>deps/</code>|Dependencies required for building the daemon|

## Pi/OS Support

<table>
<th>Pi Model</th>
<th>OS</th>
<th>OS Version</th>
<th>Tested?</th>
<th>Binary Package?</th>

<tr>
	<td rowspan=3>A+, 2, 3</td>
	<td>Raspbian</td>
	<td>Late 2015 and newer</td>
	<td><b>v</b></td>
	<td><b>v</b></td>
</tr>
<tr>
	<td>Ubuntu</td>
	<td>14.04 and newer</td>
	<td><b>v</b></td>
	<td><b>v</b></td>
</tr>
<tr>
	<td>Arch Linux ARM</td>
	<td>Late 2015 and newer</td>
	<td><b>v</b></td>
	<td><b>--</b></td>
</tr>
</table>

> **Note**: Other distributions have not been tested, but will probably also work.
> If you have successfully tested <code>rpiweatherd</code> on any unlisted platform, please contact
> me.

## Installation (Recommended)
As of version 1.1.1, binary ARMHF debian packages are available on the [Releases page](https://github.com/ronen25/rpiweatherd/releases). The .deb package will take care of all dependencies needed.
You may also compile the daemon from source.

## Compiling From Source
### Dependencies
1. **GCC version 4.6 or newer**
<br />_Required for some GNU extensions and the C99 standard._
2. **CMake version 3.0 or newer**
3. **SQLite3 development libraries version 3.5 or newer**
4. **WiringPi version 1.1 or newer**
5. **Python version 3.2 or newer**

Several development libraries must be installed in order to successfully compile <code>rpiweatherd</code>.

|Distribution Name|Command to Install|
|-----------------|------------------|
|Raspbian/Ubuntu|<code>$ sudo apt-get install cmake python3 gcc make git libsqlite3-dev wiringpi</code>|
|Arch Linux ARM|<code>$ sudo pacman -S cmake python gcc git make sqlite wiringpi</code>|

### Building and Installing
1) Clone the repository:
```
    $ git clone https://github.com/ronen25/rpiweatherd --branch 1.2-dev
```

2) Create a <code>bin</code> directory, switch to it, and initiate compilation using CMake.
```
    $ cd rpiweatherd
    $ mkdir bin && cd bin
    $ cmake -DCMAKE_BUILD_TYPE=RELEASE ..
    $ make
```

3) Finally, install the daemon:
```
    $ sudo make install
```
4) Configure <code>rpiweatherd</code> according to the [configuration guide](https://github.com/ronen25/rpiweatherd/wiki/Dameon-Configuration).

5) Run the program using the appropriate init tool, either as root or with <code>sudo</code>:

|Distribution Name|Command to Install|
|-----------------|------------------|
|Raspbian/Ubuntu|<code># service start rpiweatherd</code>|
|Arch Linux ARM|<code># systemctl enable rpiweatherd</code>|

6) Test the installation by issuing the [<code>current</code> command](https://github.com/ronen25/rpiweatherd/wiki/Getting-Data#current) from any web browser:
```
    http://[ip-of-pi]/current
```

7) Daemon administration and a reference client can be found in the <code>extra/</code> folder.

8) **Optional**: A [quick tutorial](https://github.com/ronen25/rpiweatherd/wiki/Tutorial:-Setting-Up-a-Small-Weather-Station-(From-Scratch)) for setting up rpiweatherd on the Raspberry Pi is available on the wiki.

## Clients
rpiweatherd is merely a server-side application; to communicate with it effectively (i.e. perhaps charting the data), a client-side program is needed.
This repository provides the <code>rpiweatherd-cli.py</code> client, which is a very simple, proof-of-concept client which uses a command-line interface.

I also provide a Qt-based GUI client, developed as a separate project - **[rpiweatherd-qtclient](https://github.com/ronen25/rpiweatherd-qtclient)**.

## Manual Sections
- [Installation](https://github.com/ronen25/rpiweatherd/wiki/Installation)
- [Device Support](https://github.com/ronen25/rpiweatherd/wiki/Device-Support)
- [Configuration](https://github.com/ronen25/rpiweatherd/wiki/Dameon-Configuration)
- [Running the Daemon](https://github.com/ronen25/rpiweatherd/wiki/Running-the-Daemon)
- [Getting Data](https://github.com/ronen25/rpiweatherd/wiki/Getting-Data)
- [Triggers](https://github.com/ronen25/rpiweatherd/wiki/Triggers)
- [Tutorial: Setting Up a Small Weather Station](https://github.com/ronen25/rpiweatherd/wiki/Tutorial:-Setting-Up-a-Small-Weather-Station-(From-Scratch))

***

## License
This software is licensed under the [GNU GPLv3 License](http://www.gnu.org/licenses/gpl-3.0.html).
