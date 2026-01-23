using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace ModellClientLib.Migrations
{
    public partial class AddLocomotives : Migration
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "Locomotives",
                columns: table => new
                {
                    ID = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    Name = table.Column<string>(type: "TEXT", nullable: false),
                    Address = table.Column<int>(type: "INTEGER", nullable: false),
                    Speed = table.Column<int>(type: "INTEGER", nullable: false),
                    Direction = table.Column<int>(type: "INTEGER", nullable: false),
                    Topic = table.Column<string>(type: "TEXT", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Locomotives", x => x.ID);
                });

            migrationBuilder.CreateTable(
                name: "LocoFunctions",
                columns: table => new
                {
                    ID = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    FIndex = table.Column<int>(type: "INTEGER", nullable: false),
                    Name = table.Column<string>(type: "TEXT", nullable: false),
                    IsActive = table.Column<bool>(type: "INTEGER", nullable: false),
                    LocomotiveID = table.Column<int>(type: "INTEGER", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_LocoFunctions", x => x.ID);
                    table.ForeignKey(
                        name: "FK_LocoFunctions_Locomotives_LocomotiveID",
                        column: x => x.LocomotiveID,
                        principalTable: "Locomotives",
                        principalColumn: "ID",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateIndex(
                name: "IX_LocoFunctions_LocomotiveID",
                table: "LocoFunctions",
                column: "LocomotiveID");
        }

        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "LocoFunctions");

            migrationBuilder.DropTable(
                name: "Locomotives");
        }
    }
}
