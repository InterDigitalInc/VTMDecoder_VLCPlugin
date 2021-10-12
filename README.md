/************* VLC PLUGINS FOR THE MULTITHREADED VVC DECODER BASED ON VTM  ************/

See also the README.md files inside the following subfolders for build instructions.

./DecoderPluginVLC  VVC video decoder plugin making use of the VTM decoder lib
./TsDemuxPluginVLC  TS demux with a new es vvc stream format ID

/************* Installation and usage  ************/

Tested with VLC 3.0.9.2 and 3.0.12 (Windows)

1. Install VLC

- download VLC media player.
- unpack in (for example) C:\VLC

2. Install plugin and decoder dlls

- copy decoder plugin and VTM decoder dll into plugin directory:
  ....VLC\plugins\codec\
- to play ts files, add the ts-demux plugin libvvctsdemux_plugin.dll to:
  ....VLC\plugins\demux\

3. Try to play a VVC binary file

It tries to follow frame rate from HRD information if coded, otherwise targets 50 Hz.
If playing a ts, it uses ts timestamps and find fps with consecutive timestamps.

Additional parameters control the behaviour of the decoder, and can be set either
from the command line of from the configuration dialog: go to Tools / Preferences,
select "show allow parameters" on the bottom-left, then look for Input/codecs /
Video codecs / vvcdec.

parameter description of the decoder
<parameter>				<description>
nb-threads				integer (default 0), number of threads for decoding in the range [1-32]; 0: automatic detection of cores
nb-threads-parsing		integer (default -1), Maximum number of threads for CABAC parsing (from same pool as decoding threads) [1-32]; -1: auto; 0: sequantial parsing and decoding
target-layer-set		integer (default -1), Target output layer set (for multi-layer streams)
vvc-enable-hurry-mode	bool (default true), hurry-up mode: skip decoding pictures if late
vvc-fps					float (default 0), Frames per Second; 0: try automatic, default 50Hz
