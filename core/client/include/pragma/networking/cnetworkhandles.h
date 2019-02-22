#define CLIENT_NWHANDLES \
	void HandlePacket(NetPacket &packet); \
	void HandleError(ClientEvent ev,const boost::system::error_code &err); \
	void HandleTerminate(); \
	void HandleReceiveGameInfo(NetPacket &packet); \
	void HandleReceiveServerInfo(NetPacket &packet); \
	void HandleConnect(); \
	void HandleNewPlayer(NetPacket &packet); \
	void HandleResource(NetPacket &packet); \
	void HandleResourceFragment(NetPacket &packet);