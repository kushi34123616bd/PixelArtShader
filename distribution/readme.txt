

                         ドット絵シェーダー  ver. 1.0.1
                         Pixel Art Shader


ドット絵シェーダーは Windows 環境で動作する画像作成ツールです。
Pixel Art Shader is 'image output tool' that works on Windows.

ポリゴンで作られたモデルデータを読み込み、ドット絵 風 にレンダリングした画像を出力します。
The shader loads a model data formed by polygon, and outputs image(s) that rendered LIKE pixel art.


詳細はマニュアル(manualディレクトリ内)をご覧ください。
For more information, see manual(in manual directory).
But it written in Japanese. :(



更新履歴
change log
	ver. 1.0.0  2015/02/06
		公開
		first release
	ver. 1.0.1  2016/02/22
		正射影カメラに対応した
		カメラの nearZ, farZ を設定できるようにした
		config.teco に以下の命令を追加
			camera_ortho_height float height
			camera_z_near float z
			camera_z_far float z

		add orthogonal projection camera
		be enable to set camera's near z and far z
		add commands in config.teco
			camera_ortho_height float height
			camera_z_near float z
			camera_z_far float z


---------
作者 : くし(kushi34123616bd@gmail.com)
author:kushi(kushi34123616bd@gmail.com)
web : http://kushi.lv9.org/
